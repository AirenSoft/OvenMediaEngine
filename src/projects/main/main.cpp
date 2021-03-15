//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "main.h"

#include <api_server/api_server.h>
#include <base/ovlibrary/daemon.h>
#include <base/ovlibrary/log_write.h>
#include <config/config_manager.h>
#include <mediarouter/mediarouter.h>
#include <monitoring/monitoring.h>
#include <orchestrator/orchestrator.h>
#include <providers/providers.h>
#include <publishers/publishers.h>
#include <transcode/transcoder.h>
#include <web_console/web_console.h>
#include <modules/address/address_utilities.h>

#include "banner.h"
#include "init_utilities.h"
#include "main_private.h"
#include "signals.h"
#include "third_parties.h"
#include "utilities.h"

extern bool g_is_terminated;

static ov::Daemon::State Initialize(int argc, char *argv[], ParseOption *parse_option);
static bool Uninitialize();

int main(int argc, char *argv[])
{
	ParseOption parse_option;

	{
		auto state = Initialize(argc, argv, &parse_option);
		switch (state)
		{
			case ov::Daemon::State::PARENT_SUCCESS:
				return 0;

			case ov::Daemon::State::CHILD_SUCCESS:
				// continue;
				break;

			case ov::Daemon::State::PIPE_FAIL:
				[[fallthrough]];
			case ov::Daemon::State::FORK_FAIL:
				[[fallthrough]];
			case ov::Daemon::State::PARENT_FAIL:
				[[fallthrough]];
			case ov::Daemon::State::CHILD_FAIL:
				// Failed to launch
				return 1;
		}
	}

	PrintBanner();

	auto server_config = cfg::ConfigManager::GetInstance()->GetServer();
	auto orchestrator = ocst::Orchestrator::GetInstance();
	auto monitor = mon::Monitoring::GetInstance();

	// Get public IP
	bool stun_server_parsed;
	auto stun_server_address = server_config->GetStunServer(&stun_server_parsed);
	if(stun_server_parsed)
	{
		if(ov::AddressUtilities::GetInstance()->ResolveMappedAddress(stun_server_address) == true)
		{
			logti("Resolved public IP address (%s) from stun server (%s)", ov::AddressUtilities::GetInstance()->GetMappedAddress()->GetIpAddress().CStr(), stun_server_address.CStr());
		}
		else
		{
			logtw("Could not resolve public IP address from stun server (%s)", stun_server_address.CStr());
		}
	}

	// Create info::Host list
	std::vector<info::Host> host_info_list;
	{
		// Used to check duplicate names
		std::map<ov::String, bool> vhost_map;
		auto &hosts = server_config->GetVirtualHostList();

		for (const auto &host : hosts)
		{
			auto item = vhost_map.find(host.GetName());

			if (item == vhost_map.end())
			{
				host_info_list.emplace_back(host);
				vhost_map[host.GetName()] = true;
			}
			else
			{
				logte("Duplicated VirtualHost found: %s", host.GetName().CStr());
				return 1;
			}
		}
	}

	orchestrator->ApplyOriginMap(host_info_list);

	auto api_server = api::Server::GetInstance();

	api_server->Start(server_config);

	INIT_EXTERNAL_MODULE("FFmpeg", InitializeFFmpeg);
	INIT_EXTERNAL_MODULE("SRT", InitializeSrt);
	INIT_EXTERNAL_MODULE("OpenSSL", InitializeOpenSsl);
	INIT_EXTERNAL_MODULE("SRTP", InitializeSrtp);

	//--------------------------------------------------------------------
	// Create the modules
	//--------------------------------------------------------------------
	//
	// NOTE: THE ORDER OF MODULES IS IMPORTANT
	//
	//
	// Initialize MediaRouter (MediaRouter must be registered first)
	INIT_MODULE(media_router, "MediaRouter", MediaRouter::Create());

	// Initialize Publishers
	INIT_MODULE(webrtc_publisher, "WebRTC Publisher", WebRtcPublisher::Create(*server_config, media_router));
	INIT_MODULE(hls_publisher, "HLS Publisher", HlsPublisher::Create(*server_config, media_router));
	INIT_MODULE(dash_publisher, "MPEG-DASH Publisher", DashPublisher::Create(*server_config, media_router));
	INIT_MODULE(lldash_publisher, "Low-Latency MPEG-DASH Publisher", CmafPublisher::Create(*server_config, media_router));
	INIT_MODULE(ovt_publisher, "OVT Publisher", OvtPublisher::Create(*server_config, media_router));
	INIT_MODULE(file_publisher, "File Publisher", FilePublisher::Create(*server_config, media_router));
	INIT_MODULE(rtmppush_publisher, "RtmpPush Publisher", RtmpPushPublisher::Create(*server_config, media_router));
	INIT_MODULE(thumbnail_publisher, "Thumbnail Publisher", ThumbnailPublisher::Create(*server_config, media_router));

	// Initialize Transcoder
	INIT_MODULE(transcoder, "Transcoder", Transcoder::Create(media_router));

	// Initialize Providers
	INIT_MODULE(webrtc_provider, "WebRTC Provider", pvd::WebRTCProvider::Create(*server_config, media_router));
	INIT_MODULE(mpegts_provider, "MPEG-TS Provider", pvd::MpegTsProvider::Create(*server_config, media_router));
	INIT_MODULE(rtmp_provider, "RTMP Provider", pvd::RtmpProvider::Create(*server_config, media_router));
	INIT_MODULE(ovt_provider, "OVT Provider", pvd::OvtProvider::Create(*server_config, media_router));
	INIT_MODULE(rtspc_provider, "RTSPC Provider", pvd::RtspcProvider::Create(*server_config, media_router));
	// PENDING : INIT_MODULE(rtsp_provider, "RTSP Provider", pvd::RtspProvider::Create(*server_config, media_router));

	logti("All modules are initialized successfully");

	bool should_exit = false;

	for (auto &host_info : host_info_list)
	{
		auto host_name = host_info.GetName();

		logtd("Trying to create host [%s]", host_name.CStr());
		monitor->OnHostCreated(host_info);

		// Create applications that defined by the configuration
		for (auto &app_cfg : host_info.GetApplicationList())
		{
			auto result = orchestrator->CreateApplication(host_info, app_cfg);

			switch (result)
			{
				case ocst::Result::Failed:
					logtc("Failed to create an application: %s", app_cfg.GetName().CStr());
					should_exit = true;
					break;

				case ocst::Result::Succeeded:
					break;

				case ocst::Result::Exists:
					logtc("Duplicate application [%s] found. Please check the settings.", app_cfg.GetName().CStr());
					should_exit = true;
					break;

				case ocst::Result::NotExists:
					// This should never happen
					OV_ASSERT2(false);
					logtc("Internal error occurred (THIS IS A BUG)");
					should_exit = true;
					break;
			}

			if (should_exit)
			{
				break;
			}
		}

		if (should_exit)
		{
			break;
		}
	}

	if (should_exit == false)
	{
		if (parse_option.start_service)
		{
			ov::Daemon::SetEvent();
		}

		while (g_is_terminated == false)
		{
			sleep(1);
		}
	}

	orchestrator->Release();
	// Relase all modules
	monitor->Release();
	api_server->Stop();

	RELEASE_MODULE(mpegts_provider, "MPEG-TS Provider");
	RELEASE_MODULE(rtmp_provider, "RTMP Provider");
	RELEASE_MODULE(ovt_provider, "OVT Provider");
	RELEASE_MODULE(rtspc_provider, "RTSPC Provider");
	// PENDING : RELEASE_MODULE(rtsp_provider, "RTSP Provider");

	RELEASE_MODULE(transcoder, "Transcoder");

	RELEASE_MODULE(webrtc_publisher, "WebRTC Publisher");
	RELEASE_MODULE(hls_publisher, "HLS Publisher");
	RELEASE_MODULE(dash_publisher, "MPEG-DASH Publisher");
	RELEASE_MODULE(lldash_publisher, "Low-Latency MPEG-DASH Publisher");
	RELEASE_MODULE(ovt_publisher, "OVT Publisher");
	RELEASE_MODULE(file_publisher, "File Publisher");
	RELEASE_MODULE(rtmppush_publisher, "RtmpPush Publisher");
	RELEASE_MODULE(thumbnail_publisher, "Thumbnail Publisher");

	RELEASE_MODULE(media_router, "MediaRouter");

	TERMINATE_EXTERNAL_MODULE("SRTP", TerminateSrtp);
	TERMINATE_EXTERNAL_MODULE("OpenSSL", TerminateOpenSsl);
	TERMINATE_EXTERNAL_MODULE("SRT", TerminateSrt);
	TERMINATE_EXTERNAL_MODULE("FFmpeg", TerminateFFmpeg);

	Uninitialize();

	return 0;
}

static ov::Daemon::State Initialize(int argc, char *argv[], ParseOption *parse_option)
{
	if (TryParseOption(argc, argv, parse_option) == false)
	{
		return ov::Daemon::State::PARENT_FAIL;
	}

	if (parse_option->help)
	{
		::printf("Usage: %s [OPTION]...\n", argv[0]);
		::printf("\n");
		::printf("    -c <path>   Specify a path of config files\n");
		::printf("    -v          Print OME Version\n");
		::printf("    -i          Ignores and executes the settings of %s\n", CFG_LAST_CONFIG_FILE_NAME);
		::printf("                (The JSON file is automatically generated when RESTful API is called)\n");
		return ov::Daemon::State::PARENT_FAIL;
	}

	if (parse_option->version)
	{
		::printf("OvenMediaEngine v%s\n", OME_VERSION);
		return ov::Daemon::State::PARENT_FAIL;
	}

	// Daemonize OME with start_service argument
	if (parse_option->start_service)
	{
		auto state = ov::Daemon::Initialize();

		switch (state)
		{
			case ov::Daemon::State::PARENT_SUCCESS:
				// Forked
				return state;

			case ov::Daemon::State::CHILD_SUCCESS:
				break;

			case ov::Daemon::State::PIPE_FAIL:
				[[fallthrough]];
			case ov::Daemon::State::FORK_FAIL:
				[[fallthrough]];
			case ov::Daemon::State::PARENT_FAIL:
				[[fallthrough]];
			case ov::Daemon::State::CHILD_FAIL:
				// Failed to launch
				logte("An error occurred while creating daemon");
				return state;
		}
	}

	if (InitializeSignals() == false)
	{
		logte("Could not initialize signals");
		return ov::Daemon::State::CHILD_FAIL;
	}

	ov::LogWrite::Initialize(parse_option->start_service);

	try
	{
		cfg::ConfigManager::GetInstance()->LoadConfigs(
			parse_option->config_path,
			parse_option->ignore_last_config);

		return ov::Daemon::State::CHILD_SUCCESS;
	}
	catch (std::shared_ptr<cfg::ConfigError> &error)
	{
		logte("An error occurred while load config: %s", error->ToString().CStr());
	}

	return ov::Daemon::State::CHILD_FAIL;
}

static bool Uninitialize()
{
	logti("OvenMediaEngine will be terminated");

	return true;
}