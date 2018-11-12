#include "main.h"

#include <iostream>
#include <regex>

#include <sys/utsname.h>

#include <config/config_manager.h>
#include <webrtc/webrtc_publisher.h>
#include <media_router/media_router.h>
#include <transcode/transcoder.h>
#include <rtmp/rtmp_provider.h>
#include <base/ovcrypto/ovcrypto.h>

#include <srtp2/srtp.h>

int main()
{
	struct utsname uts {};
	::uname(&uts);

	logti("OvenMediaEngine v%s is started on [%s] (%s %s - %s, %s)", OME_VERSION, uts.nodename, uts.sysname, uts.machine, uts.release, uts.version);

	logti("Trying to load configurations...");
	if(cfg::ConfigManager::Instance()->LoadConfigs() == false)
	{
		logte("An error occurred while load config");
		return 1;
	}

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
				logtd("Trying to create RTMP Provider for application [%s]...", application_info.GetName().CStr());
				providers.push_back(RtmpProvider::Create(application_info, router));

				logtd("Trying to create WebRtc Publisher for application [%s]...", application_info.GetName().CStr());
				publishers.push_back(WebRtcPublisher::Create(application_info, router));
			}
		}
		else
		{
			logtd("Nothing to do for host [%s]", host.GetName().CStr());
		}
	}

	logtd("Wait for termination...");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
	while(true)
	{
		sleep(1);
	}
#pragma clang diagnostic pop

	logtd("Trying to uninitialize OpenSSL...");
	ov::OpensslManager::ReleaseOpenSSL();

	logti("OvenMediaEngine will be terminated");

	return 0;
}
