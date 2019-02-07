#include "main.h"

#include <iostream>
#include <regex>

#include <sys/utsname.h>

#include <srtp2/srtp.h>
#include <srt/srt.h>

#include <config/config_manager.h>
#include <webrtc/webrtc_publisher.h>
#include <segment_stream/segment_stream_publisher.h>
#include <media_router/media_router.h>
#include <transcode/transcoder.h>
#include <rtmp/rtmp_provider.h>
#include <base/ovcrypto/ovcrypto.h>
#include <base/ovlibrary/stack_trace.h>

void SrtLogHandler(void *opaque, int level, const char *file, int line, const char *area, const char *message);

int main()
{
	logtd("Trying to initialize StackTrace...");
	ov::StackTrace::InitializeStackTrace(OME_VERSION);

	struct utsname uts {};
	::uname(&uts);

	logti("OvenMediaEngine v%s is started on [%s] (%s %s - %s, %s)", OME_VERSION, uts.nodename, uts.sysname, uts.machine, uts.release, uts.version);

	logti("Trying to load configurations...");
	if(cfg::ConfigManager::Instance()->LoadConfigs() == false)
	{
		logte("An error occurred while load config");
		return 1;
	}

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

	std::vector<std::shared_ptr<pvd::Provider>> providers;
	std::vector<std::shared_ptr<Publisher>> publishers;

	std::map<ov::String, std::vector<info::Application>> application_infos;

	for(auto &host : hosts)
	{
		logtd("trying to create modules for host [%s]", host.GetName().CStr());

		auto &app_info_list = application_infos[host.GetName()];

		for(const auto &application : host.GetApplications())
		{
			app_info_list.emplace_back(application);
		}

		if(app_info_list.empty() == false)
		{
			logtd("Trying to create MediaRouter for host [%s]...", host.GetName().CStr());
			router = MediaRouter::Create(app_info_list);

			logtd("Trying to create Transcoder for host [%s]...", host.GetName().CStr());
			transcoder = Transcoder::Create(app_info_list, router);

			for(const auto &application_info : app_info_list)
			{
				if(application_info.GetType() == cfg::ApplicationType::Live)
				{
					logtd("Trying to create RTMP Provider for application [%s]...", application_info.GetName().CStr());
					providers.push_back(RtmpProvider::Create(application_info, router));
				}

				auto publisher_list = application_info.GetPublishers();
				bool segment_publisher_create = false;  // Dash + HLS --> segment_publisher

				for(const auto &publisher : publisher_list)
				{
					if(publisher->IsParsed())
					{
						auto application = router->GetRouteApplicationById(application_info.GetId());

						switch(publisher->GetType())
						{
							case cfg::PublisherType::Webrtc:
								logtd("Trying to create WebRtc Publisher for application [%s]...", application_info.GetName().CStr());
								publishers.push_back(WebRtcPublisher::Create(application_info, router, application));
								break;

							case cfg::PublisherType::Dash:
							case cfg::PublisherType::Hls:
								if(!segment_publisher_create)
								{
									logtd("Trying to create SegmentStream Publisher for application [%s]...", application_info.GetName().CStr());
									publishers.push_back(SegmentStreamPublisher::Create(application_info, router));
									segment_publisher_create = true;
								}

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
			logtd("Nothing to do for host [%s]", host.GetName().CStr());
		}
	}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
	while(true)
	{
		sleep(1);
	}
#pragma clang diagnostic pop

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

	switch(level)
	{
		case logging::LogLevel::debug:
			ov_log_internal(OVLogLevelDebug, "SRT", file, line, area, "%s", mess.CStr());
			break;

		case logging::LogLevel::note:
			ov_log_internal(OVLogLevelInformation, "SRT", file, line, area, "%s", mess.CStr());
			break;

		case logging::LogLevel::warning:
			ov_log_internal(OVLogLevelWarning, "SRT", file, line, area, "%s", mess.CStr());
			break;

		case logging::LogLevel::error:
			ov_log_internal(OVLogLevelError, "SRT", file, line, area, "%s", mess.CStr());
			break;

		case logging::LogLevel::fatal:
			ov_log_internal(OVLogLevelCritical, "SRT", file, line, area, "%s", mess.CStr());
			break;

		default:
			ov_log_internal(OVLogLevelError, "SRT", file, line, area, "(Unknown level: %d) %s", level, mess.CStr());
			break;
	}
}
