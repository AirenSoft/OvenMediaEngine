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
#include <base/info/ome_version.h>
#include <base/ovlibrary/daemon.h>
#include <base/ovlibrary/log_write.h>
#include <config/config_manager.h>
#include <mediarouter/mediarouter.h>
#include <modules/address/address_utilities.h>
#include <modules/sdp/sdp_regex_pattern.h>
#include <monitoring/monitoring.h>
#include <orchestrator/orchestrator.h>
#include <providers/providers.h>
#include <publishers/publishers.h>
#include <sys/utsname.h>
#include <transcoder/transcoder.h>
#include <web_console/web_console.h>

#include "banner.h"
#include "init_utilities.h"
#include "main_private.h"
#include "signals.h"
#include "third_parties.h"
#include "utilities.h"

extern bool g_is_terminated;

static ov::Daemon::State Initialize(int argc, char *argv[], ParseOption *parse_option);
static void CheckKernelVersion();
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
	CheckKernelVersion();

	auto server_config = cfg::ConfigManager::GetInstance()->GetServer();
	auto orchestrator = ocst::Orchestrator::GetInstance();

	logti("Server ID : %s", server_config->GetID().CStr());

	// Get public IP
	bool stun_server_parsed;
	auto stun_server_address = server_config->GetStunServer(&stun_server_parsed);
	if (stun_server_parsed)
	{
		if (ov::AddressUtilities::GetInstance()->ResolveMappedAddress(stun_server_address) == true)
		{
			logti("Resolved public IP address (%s) from stun server (%s)", ov::AddressUtilities::GetInstance()->GetMappedAddress()->GetIpAddress().CStr(), stun_server_address.CStr());
		}
		else
		{
			logtw("Could not resolve public IP address from stun server (%s)", stun_server_address.CStr());
		}
	}

	// Precompile SDP patterns for better performance.
	if (SDPRegexPattern::GetInstance()->Compile() == false)
	{
		OV_ASSERT(false, "SDPRegexPattern compile failed");
		return false;
	}

	bool succeeded = true;

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
	INIT_MODULE(mpegtspush_publisher, "MpegtsPush Publisher", MpegtsPushPublisher::Create(*server_config, media_router));
	INIT_MODULE(rtmppush_publisher, "RtmpPush Publisher", RtmpPushPublisher::Create(*server_config, media_router));
	INIT_MODULE(thumbnail_publisher, "Thumbnail Publisher", ThumbnailPublisher::Create(*server_config, media_router));

	// Initialize Transcoder
	INIT_MODULE(transcoder, "Transcoder", Transcoder::Create(media_router));

	// Initialize Providers
	INIT_MODULE(webrtc_provider, "WebRTC Provider", pvd::WebRTCProvider::Create(*server_config, media_router));
	INIT_MODULE(mpegts_provider, "MPEG-TS Provider", pvd::MpegTsProvider::Create(*server_config, media_router));
	INIT_MODULE(srt_provider, "SRT Provider", pvd::SrtProvider::Create(*server_config, media_router));
	INIT_MODULE(rtmp_provider, "RTMP Provider", pvd::RtmpProvider::Create(*server_config, media_router));
	INIT_MODULE(ovt_provider, "OVT Provider", pvd::OvtProvider::Create(*server_config, media_router));
	INIT_MODULE(rtspc_provider, "RTSPC Provider", pvd::RtspcProvider::Create(*server_config, media_router));
	// PENDING : INIT_MODULE(rtsp_provider, "RTSP Provider", pvd::RtspProvider::Create(*server_config, media_router));

	auto api_server = std::make_shared<api::Server>();

	if (succeeded)
	{
		logti("All modules are initialized successfully");

		if (orchestrator->StartServer(server_config))
		{
			if (api_server->Start(server_config))
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
		}
	}

	orchestrator->Release();
	api_server->Stop();

	RELEASE_MODULE(webrtc_provider, "WebRTC Provider");
	RELEASE_MODULE(mpegts_provider, "MPEG-TS Provider");
	RELEASE_MODULE(srt_provider, "SRT Provider");
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

	info::OmeVersion::GetInstance()->SetVersion(OME_VERSION, OME_GIT_VERSION);

	// Daemonize OME with start_service argument
	if (parse_option->start_service)
	{
		auto &p{parse_option->pid_path};
		auto state{p.IsEmpty() ? ov::Daemon::Initialize() : ov::Daemon::Initialize(p.CStr())};

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

	ov::LogWrite::SetAsService(parse_option->start_service);

	auto config_manager = cfg::ConfigManager::GetInstance();

	try
	{
		config_manager->LoadConfigs(parse_option->config_path);

		return ov::Daemon::State::CHILD_SUCCESS;
	}
	catch (const cfg::ConfigError &error)
	{
		logte("An error occurred while load config: %s", error.What());
	}

	return ov::Daemon::State::CHILD_FAIL;
}

static void CheckKernelVersion()
{
	utsname name{};

	if (::uname(&name) != 0)
	{
		logte("Could not obtain utsname using uname(): %s", ov::Error::CreateErrorFromErrno()->What());
		return;
	}

	ov::String release = name.release;
	auto tokens = release.Split(".");

	if (tokens.size() > 1)
	{
		auto major = ov::Converter::ToInt32(tokens[0]);
		auto minor = ov::Converter::ToInt32(tokens[1]);

		if ((major == 5) &&
			((minor >= 3) && (minor <= 6)))
		{
			logtc("Current kernel version: %s", release.CStr());
			logtc("Linux kernel version 5.3 through 5.6 have a critical bug. Please consider using a different version. (https://bugzilla.kernel.org/show_bug.cgi?id=205933)");
		}
	}
	else
	{
		logte("Could not parse kernel version: %s", release.CStr());
	}
}

static bool Uninitialize()
{
	logti("Uninitializing TCP socket pool...");
	ov::SocketPool::GetTcpPool()->Uninitialize();
	logti("Uninitializing UDP socket pool...");
	ov::SocketPool::GetUdpPool()->Uninitialize();

	logti("OvenMediaEngine will be terminated");

	return true;
}
