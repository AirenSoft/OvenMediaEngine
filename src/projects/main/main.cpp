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
#include <monitoring/monitoring.h>
#include <orchestrator/orchestrator.h>
#include <providers/providers.h>
#include <publishers/publishers.h>
#include <transcode/transcoder.h>
#include <web_console/web_console.h>

#define INIT_MODULE(variable, name, create)                                         \
	logti("Trying to create a" name "module"); 										\
                                                                                    \
	auto variable = create;                                                         \
                                                                                    \
	if (variable == nullptr)                                                        \
	{                                                                               \
		logte("Failed to initialize" name "module");								\
		return 1;                                                                   \
	}                                                                               \
                                                                                    \
	if(orchestrator->RegisterModule(variable) == false)								\
	{																				\
		logte("Failed to register" name "module");									\
		return 1;																	\
	}																				\

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
static ov::Daemon::State Initialize(int argc, char *argv[], ParseOption *parse_option);
static bool Uninitialize();

int main(int argc, char *argv[])
{
	ParseOption parse_option;

	auto result = Initialize(argc, argv, &parse_option);

	if (result == ov::Daemon::State::PARENT_SUCCESS)
	{
		return 0;
	}
	else if (result == ov::Daemon::State::CHILD_SUCCESS)
	{
		// continue
	}
	else
	{
		// false
		return 1;
	}

	PrintBanner();

	INIT_EXTERNAL_MODULE("FFmpeg", InitializeFFmpeg);
	INIT_EXTERNAL_MODULE("SRT", InitializeSrt);
	INIT_EXTERNAL_MODULE("OpenSSL", InitializeOpenSsl);
	INIT_EXTERNAL_MODULE("SRTP", InitializeSrtp);

	const bool is_service = parse_option.start_service;

	std::shared_ptr<cfg::Server> server_config = cfg::ConfigManager::Instance()->GetServer();
	auto &hosts = server_config->GetVirtualHostList();
	std::vector<std::shared_ptr<WebConsoleServer>> web_console_servers;

	std::vector<info::Host> host_info_list;
	std::map<ov::String, bool> vhost_map;

	auto orchestrator = Orchestrator::GetInstance();
	auto monitor = mon::Monitoring::GetInstance();

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
			return 1;
		}
	}

	orchestrator->ApplyOriginMap(host_info_list);
	// Create an HTTP Manager for Segment Publishers
	std::map<int, std::shared_ptr<HttpServer>> http_server_manager;

	//--------------------------------------------------------------------
	// Create the modules
	//--------------------------------------------------------------------
	// Initialize MediaRouter (MediaRouter must be registered first)
	INIT_MODULE(media_router, "MediaRouter", MediaRouter::Create());

	// Initialize Providers
	INIT_MODULE(rtmp_provider, "RTMP Provider", RtmpProvider::Create(*server_config, media_router));
	INIT_MODULE(ovt_provider, "OVT Provider", pvd::OvtProvider::Create(*server_config, media_router));
	INIT_MODULE(rtspc_provider, "RTSPC Provider", pvd::RtspcProvider::Create(*server_config, media_router));
	INIT_MODULE(rtsp_provider, "RTSP Provider", pvd::RtspProvider::Create(*server_config, media_router));

	// Initialize Transcoder
	INIT_MODULE(transcoder, "Transcoder", Transcoder::Create(media_router));

	// Initialize Publishers
	INIT_MODULE(webrtc_publisher, "WebRTC Publisher", WebRtcPublisher::Create(*server_config, media_router));
	INIT_MODULE(hls_publisher, "HLS Publisher", HlsPublisher::Create(http_server_manager, *server_config, media_router));
	INIT_MODULE(dash_publisher, "MPEG-DASH Publisher", DashPublisher::Create(http_server_manager, *server_config, media_router));
	INIT_MODULE(lldash_publisher, "Low-Latency MPEG-DASH Publisher", CmafPublisher::Create(http_server_manager, *server_config, media_router));
	INIT_MODULE(ovt_publisher, "OVT Publisher", OvtPublisher::Create(*server_config, media_router));

	logti("All modules are initialized successfully");

	for (auto &host_info : host_info_list)
	{
		auto host_name = host_info.GetName();

		logtd("Trying to create host [%s]", host_name.CStr());
		monitor->OnHostCreated(host_info);

		// Create applications that defined by the configuration
		for (auto &app_cfg : host_info.GetApplicationList())
		{
			orchestrator->CreateApplication(host_info, app_cfg);
		}
	}

	if (is_service)
	{
		ov::Daemon::SetEvent();
	}

	while (true)
	{
		sleep(5);
		//Plan to start / stop with external signals
		//mon::Monitoring::GetInstance()->ShowInfo();
	}

	Uninitialize();

	return 0;
}

static void PrintBanner()
{
	utsname uts{};
	::uname(&uts);

	logti("OvenMediaEngine v" OME_VERSION OME_GIT_VERSION_EXTRA " is started on [%s] (%s %s - %s, %s)", uts.nodename, uts.sysname, uts.machine, uts.release, uts.version);

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

static ov::Daemon::State Initialize(int argc, char *argv[], ParseOption *parse_option)
{
	if (TryParseOption(argc, argv, parse_option) == false)
	{
		return ov::Daemon::State::PARENT_FAIL;
	}

	if (parse_option->help)
	{
		::printf("Usage: %s [OPTION]...\n", argv[0]);
		::printf("    -c <path>             Specify a path of config files\n");
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

		if (state == ov::Daemon::State::PARENT_SUCCESS)
		{
			return state;
		}
		else if (state == ov::Daemon::State::CHILD_SUCCESS)
		{
			// continue
		}
		else
		{
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

	if (cfg::ConfigManager::Instance()->LoadConfigs(parse_option->config_path) == false)
	{
		logte("An error occurred while load config");
		return ov::Daemon::State::CHILD_FAIL;
	}

	return ov::Daemon::State::CHILD_SUCCESS;
}

static bool Uninitialize()
{
	logtd("Trying to uninitialize OpenSSL...");
	ov::OpensslManager::ReleaseOpenSSL();

	::srt_cleanup();

	logti("OvenMediaEngine will be terminated");

	return true;
}