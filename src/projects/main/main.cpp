#include "main.h"

#include <iostream>
#include <regex>

#include <sys/utsname.h>

#include <srtp2/srtp.h>
#include <srt/srt.h>

#include <config/config_manager.h>
#include <webrtc/webrtc_publisher.h>
#include <dash/dash_publisher.h>
#include <hls/hls_publisher.h>
#include <media_router/media_router.h>
#include <transcode/transcoder.h>
#include <monitoring/monitoring_server.h>
#include <web_console/web_console.h>
#include <rtmp/rtmp_provider.h>
#include <base/ovcrypto/ovcrypto.h>
#include <base/ovlibrary/stack_trace.h>
#include <base/ovlibrary/log_write.h>

void SrtLogHandler(void *opaque, int level, const char *file, int line, const char *area, const char *message);

struct ParseOption
{
	// -c <config_path>
	ov::String config_path = "";

	// -s start with systemctl
    bool start_service = false;
};

bool TryParseOption(int argc, char *argv[], ParseOption *parse_option)
{
	constexpr const char *opt_string = "hvt:c:s";

	while(true)
	{
		int name = getopt(argc, argv, opt_string);

		switch(name)
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

			case 's':
			    // Don't use this option manually
				parse_option->start_service = true;
				break;

			default: // '?'
				// invalid argument
				return false;
		}
	}
}

int main(int argc, char *argv[])
{
	ParseOption parse_option;

	if(TryParseOption(argc, argv, &parse_option) == false)
	{
		return 1;
	}

	ov::StackTrace::InitializeStackTrace(OME_VERSION);

    ov::LogWrite::Initialize(parse_option.start_service);

	if(cfg::ConfigManager::Instance()->LoadConfigs(parse_option.config_path) == false)
	{
		logte("An error occurred while load config");
		return 1;
	}

	struct utsname uts {};
	::uname(&uts);

	logti("OvenMediaEngine v%s is started on [%s] (%s %s - %s, %s)", OME_VERSION, uts.nodename, uts.sysname, uts.machine, uts.release, uts.version);

	logtd("Trying to initialize SRT...");
	srt_startup();
	srt_setloglevel(logging::LogLevel::debug);
	srt_setloghandler(nullptr, SrtLogHandler);

	logtd("Trying to initialize OpenSSL...");
	ov::OpensslManager::InitializeOpenssl();

	logtd("Trying to initialize libsrtp...");
	int err = srtp_init();
	if(err != srtp_err_status_ok)
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

	for(auto &host : hosts)
	{
		auto host_name = host.GetName();

		logtd("Trying to create modules for host [%s]", host_name.CStr());

		auto &app_info_list = application_infos[host.GetName()];

		for(const auto &application : host.GetApplications())
		{
			logti("Trying to create application [%s] (%s)...", application.GetName().CStr(), application.GetTypeName().CStr());

			app_info_list.emplace_back(application);
		}

		if(app_info_list.empty() == false)
		{
			logti("Trying to create MediaRouter for host [%s]...", host_name.CStr());
			router = MediaRouter::Create(app_info_list);

			logti("Trying to create Transcoder for host [%s]...", host_name.CStr());
			transcoder = Transcoder::Create(app_info_list, router);

			for(const auto &application_info : app_info_list)
			{
				auto app_name = application_info.GetName();

				if(application_info.GetType() == cfg::ApplicationType::Live)
				{
					logti("Trying to create RTMP Provider for application [%s/%s]...", host_name.CStr(), app_name.CStr());
					providers.push_back(RtmpProvider::Create(&application_info, router));
				}

				if(application_info.GetWebConsole().IsParsed())
				{
					logti("Trying to initialize WebConsole for application [%s/%s]...", host_name.CStr(), app_name.CStr());
					web_console_servers.push_back(WebConsoleServer::Create(&application_info));
				}

				auto publisher_list = application_info.GetPublishers().GetPublisherList();
				std::map<int, std::shared_ptr<HttpServer>> segment_http_server_manager; // key : port number

				for(const auto &publisher : publisher_list)
				{
					if(publisher->IsParsed())
					{
						auto application = router->GetRouteApplicationById(application_info.GetId());

						switch(publisher->GetType())
						{
							case cfg::PublisherType::Webrtc:
								logti("Trying to create WebRTC Publisher for application [%s/%s]...", host_name.CStr(), app_name.CStr());
								publishers.push_back(WebRtcPublisher::Create(&application_info, router, application));
								break;

							case cfg::PublisherType::Dash:
								logti("Trying to create DASH Publisher for application [%s/%s]...", host_name.CStr(), app_name.CStr());
								publishers.push_back(DashPublisher::Create(segment_http_server_manager,
								                                           &application_info,
								                                           router));
								break;

							case cfg::PublisherType::Hls:
								logti("Trying to create HLS Publisher for application [%s/%s]...", host_name.CStr(), app_name.CStr());
								publishers.push_back(HlsPublisher::Create(segment_http_server_manager,
								                                          &application_info,
								                                          router));
								break;

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

//		// Monitoring Server
//        auto monitoring_port = host.GetPorts().GetMonitoringPort();
//        if(monitoring_port.IsParsed() && (providers.size() > 0 || publishers.size() > 0))
//        {
//            logtd("Monitoring Server Start - host[%s] port[%d]", host_name.CStr(), monitoring_port.GetPort());
//
//            monitoring_server = std::make_shared<MonitoringServer>();
//            monitoring_server->Start(ov::SocketAddress(static_cast<uint16_t>(monitoring_port.GetPort())),
//                                     providers,
//                                     publishers);
//        }
	}

	while(true)
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

	if(std::regex_search(m, matches, std::regex("^([0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{6})([/a-zA-Z:.!*_]+) ([/a-zA-Z:.!]+ )?(.+)")))
	{
		mess = std::string(matches[4]).c_str();
	}
	else
	{
		// Unknown pattern
		mess = message;
	}

	const char *SRT_LOG_TAG = "SRT";

	switch(level)
	{
		case logging::LogLevel::debug:
			ov_log_internal(OVLogLevelDebug, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case logging::LogLevel::note:
			ov_log_internal(OVLogLevelInformation, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case logging::LogLevel::warning:
			ov_log_internal(OVLogLevelWarning, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case logging::LogLevel::error:
			ov_log_internal(OVLogLevelError, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case logging::LogLevel::fatal:
			ov_log_internal(OVLogLevelCritical, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		default:
			ov_log_internal(OVLogLevelError, SRT_LOG_TAG, file, line, area, "(Unknown level: %d) %s", level, mess.CStr());
			break;
	}
}
