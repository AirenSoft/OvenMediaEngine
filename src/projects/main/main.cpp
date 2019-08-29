//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "main.h"

#include <iostream>
#include <regex>

#include <sys/utsname.h>

#include <srt/srt.h>
#include <srtp2/srtp.h>

#include <base/ovcrypto/ovcrypto.h>
#include <base/ovlibrary/daemon.h>
#include <base/ovlibrary/log_write.h>
#include <base/ovlibrary/stack_trace.h>
#include <config/config_manager.h>
#include <media_router/media_router.h>
#include <monitoring/monitoring_server.h>
#include <publishers/publishers.h>
#include <rtmp/rtmp_provider.h>
#include <transcode/transcoder.h>
#include <web_console/web_console.h>
#include <webrtc/webrtc_publisher.h>

extern "C"
{
#include <libavutil/ffversion.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#define CHECK_FAIL(x) \
    if((x) == nullptr) \
    { \
        srt_cleanup(); \
		\
		if(parse_option.start_service) \
		{ \
        	ov::Daemon::SetEvent(false); \
		} \
        return 1; \
    }

void SrtLogHandler(void *opaque, int level, const char *file, int line, const char *area, const char *message);

struct ParseOption
{
	// -c <config_path>
	ov::String config_path = "";

	// -s start with systemctl
	bool start_service = false;
};

ov::String GetFFmpegVersion(int version)
{
	return ov::String::FormatString("%d.%d.%d",
									AV_VERSION_MAJOR(version),
									AV_VERSION_MINOR(version),
									AV_VERSION_MICRO(version));
}

bool TryParseOption(int argc, char *argv[], ParseOption *parse_option)
{
	constexpr const char *opt_string = "hvt:c:d";

	while (true)
	{
		int name = getopt(argc, argv, opt_string);

		switch (name)
		{
			case -1:
				// end of arguments
				return true;

			case 'h':
				printf("Usage: %s [OPTION]...\n", argv[0]);
				printf("    -c <path>             Specify a path of config files\n");
				return false;

			case 'v':
				printf("OvenMediaEngine v%s\n", OME_VERSION);
				return false;

			case 'c':
				parse_option->config_path = optarg;
				break;

			case 'd':
				// Don't use this option manually
				parse_option->start_service = true;
				break;

			default:  // '?'
				// invalid argument
				return false;
		}
	}
}

int main(int argc, char *argv[])
{
	ParseOption parse_option;

	if (TryParseOption(argc, argv, &parse_option) == false)
	{
		return 1;
	}

	// Daemonize OME with start_service argument
	if (parse_option.start_service)
	{
		auto state = ov::Daemon::Initialize();

		if (state == ov::Daemon::State::PARENT_SUCCESS)
		{
			return 0;
		}
		else if (state == ov::Daemon::State::CHILD_SUCCESS)
		{
			// continue
		}
		else
		{
			logte("An error occurred while creating daemon");
			return 1;
		}
	}

	ov::StackTrace::InitializeStackTrace(OME_VERSION);

	ov::LogWrite::Initialize(parse_option.start_service);

	if (cfg::ConfigManager::Instance()->LoadConfigs(parse_option.config_path) == false)
	{
		logte("An error occurred while load config");
		return 1;
	}

	utsname uts{};
	::uname(&uts);

	logti("OvenMediaEngine v%s is started on [%s] (%s %s - %s, %s)", OME_VERSION, uts.nodename, uts.sysname, uts.machine, uts.release, uts.version);

	logti("With modules:");
	logti("  FFmpeg %s", FFMPEG_VERSION);
	logti("    Configuration: %s", avutil_configuration());
	logti("    libavformat: %s", GetFFmpegVersion(avformat_version()).CStr());
	logti("    libavcodec: %s", GetFFmpegVersion(avcodec_version()).CStr());
	logti("    libavfilter: %s", GetFFmpegVersion(avfilter_version()).CStr());
	logti("    libswresample: %s", GetFFmpegVersion(swresample_version()).CStr());
	logti("    libswscale: %s", GetFFmpegVersion(swscale_version()).CStr());
	logti("  SRT: %s", SRT_VERSION_STRING);
	logti("  OpenSSL: %s", OpenSSL_version(OPENSSL_VERSION));
	logti("    Configuration: %s", OpenSSL_version(OPENSSL_CFLAGS));

	logtd("Trying to initialize SRT...");
	srt_startup();
	srt_setloglevel(srt_logging::LogLevel::debug);
	srt_setloghandler(nullptr, SrtLogHandler);

	logtd("Trying to initialize OpenSSL...");
	ov::OpensslManager::InitializeOpenssl();

	logtd("Trying to initialize libsrtp...");
	int err = srtp_init();
	if (err != srtp_err_status_ok)
	{
		loge("SRTP", "Could not initialize SRTP (err: %d)", err);
		return 1;
	}

	std::shared_ptr<cfg::Server> server = cfg::ConfigManager::Instance()->GetServer();
	std::vector<cfg::Host> hosts = server->GetHosts();

	std::shared_ptr<MediaRouter> router;
	std::shared_ptr<Transcoder> transcoder;
	std::vector<std::shared_ptr<WebConsoleServer>> web_console_servers;

	std::vector<std::shared_ptr<pvd::Provider>> providers;
	std::vector<std::shared_ptr<Publisher>> publishers;
	//std::shared_ptr<MonitoringServer> monitoring_server;

	std::map<ov::String, std::vector<info::Application>> application_infos;

	for (auto &host : hosts)
	{
		auto host_name = host.GetName();

		logtd("Trying to create modules for host [%s]", host_name.CStr());

		auto &app_info_list = application_infos[host.GetName()];

		for (const auto &application : host.GetApplications())
		{
			logti("Trying to create application [%s] (%s)...", application.GetName().CStr(), application.GetTypeName().CStr());

			app_info_list.emplace_back(application);
		}

		if (app_info_list.empty() == false)
		{
			logti("Trying to create MediaRouter for host [%s]...", host_name.CStr());
			router = MediaRouter::Create(app_info_list);
			CHECK_FAIL(router);

			logti("Trying to create Transcoder for host [%s]...", host_name.CStr());
			transcoder = Transcoder::Create(app_info_list, router);
			CHECK_FAIL(transcoder);

			for (const auto &application_info : app_info_list)
			{
				auto app_name = application_info.GetName();

				if (application_info.GetType() == cfg::ApplicationType::Live)
				{
					logti("Trying to create RTMP Provider for application [%s/%s]...", host_name.CStr(), app_name.CStr());
					auto provider = RtmpProvider::Create(&application_info, router);
					CHECK_FAIL(provider);
					providers.push_back(provider);
				}

				if (application_info.GetWebConsole().IsParsed())
				{
					logti("Trying to initialize WebConsole for application [%s/%s]...", host_name.CStr(), app_name.CStr());
					auto console = WebConsoleServer::Create(&application_info);
					CHECK_FAIL(console);
					web_console_servers.push_back(console);
				}

				auto publisher_list = application_info.GetPublishers().GetPublisherList();
				std::map<int, std::shared_ptr<HttpServer>> segment_http_server_manager;  // key : port number

				for (const auto &publisher : publisher_list)
				{
					if (publisher->IsParsed())
					{
						auto application = router->GetRouteApplicationById(application_info.GetId());

						switch (publisher->GetType())
						{
							case cfg::PublisherType::Webrtc:
							{
								logti("Trying to create WebRTC Publisher for application [%s/%s]...", host_name.CStr(), app_name.CStr());
								auto webrtc = WebRtcPublisher::Create(&application_info, router, application);
								CHECK_FAIL(webrtc);
								publishers.push_back(webrtc);
								break;
							}

							case cfg::PublisherType::Dash:
							{
								logti("Trying to create DASH Publisher for application [%s/%s]...", host_name.CStr(), app_name.CStr());
								auto dash = DashPublisher::Create(segment_http_server_manager, &application_info, router);
								CHECK_FAIL(dash);
								publishers.push_back(dash);
								break;
							}

							case cfg::PublisherType::Hls:
							{
								logti("Trying to create HLS Publisher for application [%s/%s]...", host_name.CStr(), app_name.CStr());
								auto hls = HlsPublisher::Create(segment_http_server_manager, &application_info, router);
								CHECK_FAIL(hls);
								publishers.push_back(hls);
								break;
							}

							case cfg::PublisherType::Cmaf:
							{
								logti("Trying to create CMAF Publisher for application [%s/%s]...", host_name.CStr(), app_name.CStr());
								auto cmaf = CmafPublisher::Create(segment_http_server_manager, &application_info, router);
								CHECK_FAIL(cmaf);
								publishers.push_back(cmaf);
								break;
							}

							case cfg::PublisherType::Rtmp:
							default:
								// not implemented
								break;
						}
					}
				}
			}
		}
		else
		{
			logtw("Nothing to do for host [%s]", host_name.CStr());
		}
	}

	if (parse_option.start_service)
	{
		ov::Daemon::SetEvent();
	}

	while (true)
	{
		sleep(1);
	}

	logtd("Trying to uninitialize OpenSSL...");
	ov::OpensslManager::ReleaseOpenSSL();

	srt_cleanup();

	logti("OvenMediaEngine will be terminated");

	return 0;
}

void SrtLogHandler(void *opaque, int level, const char *file, int line, const char *area, const char *message)
{
	// SRT log format:
	// HH:mm:ss.ssssss/SRT:xxxx:xxxxxx.N: xxx.c: .................
	// 13:20:15.618019.N: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317
	// 13:20:15.618019.!!FATAL!!: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317
	// 13:20:15.618019.!W: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317
	// 13:20:15.618019.*E: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317
	// 20:41:11.929158/ome_origin*E: SRT.d: SND-DROPPED 1 packets - lost delaying for 1021m
	// 13:20:15.618019/SRT:RcvQ:worker.N: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317

	std::smatch matches;
	std::string m = message;
	ov::String mess;

	if (std::regex_search(m, matches, std::regex("^([0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{6})([/a-zA-Z:.!*_]+) ([/a-zA-Z:.!]+ )?(.+)")))
	{
		mess = std::string(matches[4]).c_str();
	}
	else
	{
		// Unknown pattern
		mess = message;
	}

	const char *SRT_LOG_TAG = "SRT";

	switch (level)
	{
		case srt_logging::LogLevel::debug:
			ov_log_internal(OVLogLevelDebug, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case srt_logging::LogLevel::note:
			ov_log_internal(OVLogLevelInformation, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case srt_logging::LogLevel::warning:
			ov_log_internal(OVLogLevelWarning, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case srt_logging::LogLevel::error:
			ov_log_internal(OVLogLevelError, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case srt_logging::LogLevel::fatal:
			ov_log_internal(OVLogLevelCritical, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		default:
			ov_log_internal(OVLogLevelError, SRT_LOG_TAG, file, line, area, "(Unknown level: %d) %s", level, mess.CStr());
			break;
	}
}
