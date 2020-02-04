//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "main.h"
#include "./signals.h"
#include "./third_parties.h"
#include "./utilities.h"
#include "main_private.h"

#include <sys/utsname.h>

#include <base/ovlibrary/daemon.h>
#include <base/ovlibrary/log_write.h>
#include <config/config_manager.h>

#include <media_router/media_router.h>
#include <orchestrator/orchestrator.h>
#include <monitoring/monitoring.h>	
#include <providers/providers.h>
#include <publishers/publishers.h>
#include <transcode/transcoder.h>
#include <web_console/web_console.h>

#define INIT_MODULE(variable, name, create)                                         \
	logti("Trying to create a module " name " for host [%s]...", host_name.CStr()); \
	auto variable = create;                                                         \
	if (variable == nullptr)                                                        \
	{                                                                               \
		initialized = false;                                                        \
		break;                                                                      \
	}

#define INIT_EXTERNAL_MODULE(name, func)                                          \
	{                                                                             \
		logtd("Trying to initialize " name "...");                                \
		auto error = func();                                                      \
                                                                                  \
		if (error != nullptr)                                                     \
		{                                                                         \
			logte("Could not initialize " name ": %s", error->ToString().CStr()); \
			return 1;                                                             \
		}                                                                         \
	}

static void PrintBanner();
static bool Initialize(int argc, char *argv[], ParseOption *parse_option);
static bool Uninitialize();

int main(int argc, char *argv[])
{
	ParseOption parse_option;

	if (Initialize(argc, argv, &parse_option) == false)
	{
		return 1;
	}

	PrintBanner();

	INIT_EXTERNAL_MODULE("SRT", InitializeSrt);
	INIT_EXTERNAL_MODULE("OpenSSL", InitializeOpenSsl);
	INIT_EXTERNAL_MODULE("SRTP", InitializeSrtp);

	const bool is_service = parse_option.start_service;

	std::shared_ptr<cfg::Server> server_config = cfg::ConfigManager::Instance()->GetServer();
	std::vector<cfg::VirtualHost> hosts = server_config->GetVirtualHostList();

	bool initialized = false;
	std::vector<std::shared_ptr<WebConsoleServer>> web_console_servers;

	std::vector<info::Host> host_info_list;
	std::map<ov::String, bool> vhost_map;

	auto orchestrator = Orchestrator::GetInstance();
	auto monitor = mon::Monitoring::GetInstance();
	bool succeeded = true;

	// Create info::Host
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
			succeeded = false;

			break;
		}
	}

	if (succeeded)
	{
		orchestrator->ApplyOriginMap(host_info_list);

		for (auto &host_info : host_info_list)
		{
			auto host_name = host_info.GetName();

			monitor->OnHostCreated(host_info);

			//////////////////////////////
			// Host Level Modules
			//////////////////////////////

			if (initialized == false)
			{
				logtd("Trying to create modules for host [%s]", host_name.CStr());

				do
				{
					initialized = true;

					// Create an HTTP Manager for Segment Publishers
					std::map<int, std::shared_ptr<HttpServer>> http_server_manager;

					//--------------------------------------------------------------------
					// Create the modules
					//--------------------------------------------------------------------
					INIT_MODULE(media_router, "MediaRouter", MediaRouter::Create());
					INIT_MODULE(transcoder, "Transcoder", Transcoder::Create(media_router));
					INIT_MODULE(rtmp_provider, "RTMP Provider", RtmpProvider::Create(*server_config, media_router));
					INIT_MODULE(ovt_provider, "OVT Provider", pvd::OvtProvider::Create(*server_config, media_router));
					INIT_MODULE(rtsp_provider, "RTSP Provider", pvd::RtspProvider::Create(*server_config, media_router));
					INIT_MODULE(rtspc_provider, "RTSPC Provider", pvd::RtspcProvider::Create(*server_config, media_router));
					INIT_MODULE(webrtc_publisher, "WebRTC Publisher", WebRtcPublisher::Create(*server_config, host_info, media_router));
					INIT_MODULE(hls_publisher, "HLS Publisher", HlsPublisher::Create(http_server_manager, *server_config, host_info, media_router));
					INIT_MODULE(dash_publisher, "MPEG-DASH Publisher", DashPublisher::Create(http_server_manager, *server_config, host_info, media_router));
					INIT_MODULE(lldash_publisher, "Low-Latency MPEG-DASH Publisher", CmafPublisher::Create(http_server_manager, *server_config, host_info, media_router));
					INIT_MODULE(ovt_publisher, "OVT Publisher", OvtPublisher::Create(*server_config, host_info, media_router));

					//--------------------------------------------------------------------
					// Register modules to Orchestrator
					//--------------------------------------------------------------------
					// Currently, MediaRouter must be registered first
					// Register media router
					initialized = initialized && orchestrator->RegisterModule(media_router);
					// Register providers
					initialized = initialized && orchestrator->RegisterModule(rtmp_provider);
					initialized = initialized && orchestrator->RegisterModule(ovt_provider);
					initialized = initialized && orchestrator->RegisterModule(rtsp_provider);
					initialized = initialized && orchestrator->RegisterModule(rtspc_provider);
					// Register transcoder
					initialized = initialized && orchestrator->RegisterModule(transcoder);
					// Register publishers
					initialized = initialized && orchestrator->RegisterModule(webrtc_publisher);
					initialized = initialized && orchestrator->RegisterModule(hls_publisher);
					initialized = initialized && orchestrator->RegisterModule(dash_publisher);
					initialized = initialized && orchestrator->RegisterModule(lldash_publisher);
					initialized = initialized && orchestrator->RegisterModule(ovt_publisher);
				} while (false);

				if (initialized)
				{
					logti("All modules are initialized successfully");
				}
				else
				{
					logte("Failed to initialize module");
					succeeded = false;
				}
			}

			// Create applications that defined by the configuration
			for (auto &app_cfg : host_info.GetApplicationList())
			{
				orchestrator->CreateApplication(host_info, app_cfg);
			}
		}
	}

	if (succeeded)
	{
		if (is_service)
		{
			ov::Daemon::SetEvent(succeeded);
		}

		while (true)
		{
			sleep(1);
		}
	}

	Uninitialize();

	return 0;
}

static void PrintBanner()
{
	utsname uts{};
	::uname(&uts);

	logti("OvenMediaEngine v%s is started on [%s] (%s %s - %s, %s)", OME_VERSION, uts.nodename, uts.sysname, uts.machine, uts.release, uts.version);

	logti("With modules:");
	logti("  FFmpeg %s", GetFFmpegVersion());
	logti("    Configuration: %s", GetFFmpegConfiguration());
	logti("    libavformat: %s", GetFFmpegAvFormatVersion());
	logti("    libavcodec: %s", GetFFmpegAvCodecVersion());
	logti("    libavutil: %s", GetFFmpegAvUtilVersion());
	logti("    libavfilter: %s", GetFFmpegAvFilterVersion());
	logti("    libswresample: %s", GetFFmpegSwResampleVersion());
	logti("    libswscale: %s", GetFFmpegSwScaleVersion());
	logti("  SRT: %s", GetSrtVersion());
	logti("  SRTP: %s", GetSrtpVersion());
	logti("  OpenSSL: %s", GetOpenSslVersion());
	logti("    Configuration: %s", GetOpenSslConfiguration());
}

static bool Initialize(int argc, char *argv[], ParseOption *parse_option)
{
	if (TryParseOption(argc, argv, parse_option) == false)
	{
		return false;
	}

	if (parse_option->help)
	{
		::printf("Usage: %s [OPTION]...\n", argv[0]);
		::printf("    -c <path>             Specify a path of config files\n");
		return false;
	}

	if (parse_option->version)
	{
		::printf("OvenMediaEngine v%s\n", OME_VERSION);
		return false;
	}

	// Daemonize OME with start_service argument
	if (parse_option->start_service)
	{
		auto state = ov::Daemon::Initialize();

		if (state == ov::Daemon::State::PARENT_SUCCESS)
		{
			return false;
		}
		else if (state == ov::Daemon::State::CHILD_SUCCESS)
		{
			// continue
		}
		else
		{
			logte("An error occurred while creating daemon");
			return false;
		}
	}

	if (InitializeSignals() == false)
	{
		logte("Could not initialize signals");
		return false;
	}

	ov::LogWrite::Initialize(parse_option->start_service);

	if (cfg::ConfigManager::Instance()->LoadConfigs(parse_option->config_path) == false)
	{
		logte("An error occurred while load config");
		return false;
	}

	return true;
}

static bool Uninitialize()
{
	logtd("Trying to uninitialize OpenSSL...");
	ov::OpensslManager::ReleaseOpenSSL();

	::srt_cleanup();

	logti("OvenMediaEngine will be terminated");

	return true;
}