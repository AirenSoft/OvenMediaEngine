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
#include <monitoring/monitoring_server.h>
#include <orchestrator/orchestrator.h>
#include <providers/providers.h>
#include <publishers/publishers.h>
#include <transcode/transcoder.h>
#include <web_console/web_console.h>

#define CHECK_FAIL(x)                    \
	if ((x) == nullptr)                  \
	{                                    \
		::srt_cleanup();                 \
                                         \
		if (parse_option.start_service)  \
		{                                \
			ov::Daemon::SetEvent(false); \
		}                                \
		return 1;                        \
	}

#define INIT_MODULE(name, func)                                                   \
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

	INIT_MODULE("SRT", InitializeSrt);
	INIT_MODULE("OpenSSL", InitializeOpenSsl);
	INIT_MODULE("SRTP", InitializeSrtp);

	std::shared_ptr<cfg::Server> server_config = cfg::ConfigManager::Instance()->GetServer();
	std::vector<cfg::VirtualHost> hosts = server_config->GetVirtualHostList();

	std::shared_ptr<MediaRouter> router;
	std::shared_ptr<Transcoder> transcoder;
	std::vector<std::shared_ptr<WebConsoleServer>> web_console_servers;

	std::vector<info::Host> host_info_list;

	auto orchestrator = Orchestrator::GetInstance();

	// Create info::Host
	for (const auto &host : hosts)
	{
		logti("Trying to create host info [%s]...", host.GetName().CStr());
		host_info_list.emplace_back(host);
	}

	orchestrator->ApplyOriginMap(hosts);

	for (auto &host_info : host_info_list)
	{
		auto host_name = host_info.GetName();

		logtd("Trying to create modules for host [%s]", host_name.CStr());

		//////////////////////////////
		// Host Level Modules
		//TODO(Getroot): Support Virtual Host Function. This code assumes that there is only one Host.
		//////////////////////////////

		logti("Trying to create MediaRouter for host [%s]...", host_name.CStr());
		router = MediaRouter::Create();
		CHECK_FAIL(router);

		logti("Trying to create Transcoder for host [%s]...", host_name.CStr());
		transcoder = Transcoder::Create(router);
		CHECK_FAIL(transcoder);

		logti("Trying to create RTMP Provider [%s]...", host_name.CStr());
		auto rtmp_provider = RtmpProvider::Create(*server_config, host_info, router);
		CHECK_FAIL(rtmp_provider);

		logti("Trying to create OVT Provider [%s]...", host_name.CStr());
		auto ovt_provider = pvd::OvtProvider::Create(*server_config, host_info, router);
		CHECK_FAIL(ovt_provider);

		logti("Trying to create WebRTC Publisher [%s]...", host_name.CStr());
		auto webrtc_publisher = WebRtcPublisher::Create(*server_config, host_info, router);
		CHECK_FAIL(webrtc_publisher);

		logti("Trying to create OVT Publisher [%s]...", host_name.CStr());
		auto ovt_publisher = OvtPublisher::Create(*server_config, host_info, router);
		CHECK_FAIL(ovt_publisher);

		//--------------------------------------------------------------------
		// Register modules to Orchestrator
		//--------------------------------------------------------------------
		// Currently, MediaRouter must be registered first
		// Register media router
		orchestrator->RegisterModule(router);
		// Register providers
		orchestrator->RegisterModule(rtmp_provider);
		orchestrator->RegisterModule(ovt_provider);
		// Register transcoder
		orchestrator->RegisterModule(transcoder);
		// Register publishers
		orchestrator->RegisterModule(webrtc_publisher);
		orchestrator->RegisterModule(ovt_publisher);

		// Create applications that defined by the configuration
		for (auto app_cfg : host_info.GetApplicationList())
		{
			orchestrator->CreateApplication(host_name, app_cfg);
		}

#if 0
		/* For Edge Test */
		info::Application app_info(123, "app2", cfg::Application());
		orchestrator->CreateApplication(app_info);
		while (true)
		{
			if (ovt_provider->PullStream(app_info, "edge", {"ovt://192.168.0.199:9000/app/stream_o"}))
			{
				logte("Pull stream is completed");
				break;
			}

			sleep(1);
		}
#endif
	}

	if (parse_option.start_service)
	{
		ov::Daemon::SetEvent();
	}

	while (true)
	{
		sleep(1);
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