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

int main(int argc, char *argv[])
{
	struct utsname uts;
	::uname(&uts);

	logti("OvenMediaEngine v" OME_VERSION " is started on [%s] (%s %s - %s, %s)", uts.nodename, uts.sysname, uts.machine, uts.release, uts.version);

	logti("Trying to load configurations...");
	if(ConfigManager::Instance()->LoadConfigs() == false)
	{
		logte("An error occurred while load config");
		return 1;
	}

	logtd("Trying to initialize OpenSSL...");
	OpenSSLManager::InitializeOpenSSL();

	logtd("Trying to initialize libsrtp...");
	int err = srtp_init();
	if(err != srtp_err_status_ok)
	{
		loge("SRTP", "Could not initialize SRTP (err: %d)", err);
		return 1;
	}

	logtd("Trying to create MediaRouter...");
	auto router = MediaRouter::Create();

	logtd("Trying to create Transcoder...");
	auto transcoder = Transcoder::Create(router);

	logtd("Trying to create RTMP Provider...");
	auto provider = RtmpProvider::Create(router);

	logtd("Trying to create WebRtc Publisher...");
	auto publisher = WebRtcPublisher::Create(router);

	// Wait for termination...
	while(true)
	{
		sleep(1);
	}

	logtd("Trying to uninitialize OpenSSL...");
	OpenSSLManager::ReleaseOpenSSL();

	logti("OvenMediaEngine will be terminated");

	return 0;
}