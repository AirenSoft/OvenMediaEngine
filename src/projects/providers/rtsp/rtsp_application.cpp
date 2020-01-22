#include "rtsp_application.h"
#include "rtsp_stream.h"

#include "base/provider/application.h"
#include "base/application/stream_info.h"

#define OV_LOG_TAG "RtspApplication"

std::shared_ptr<RtspApplication> RtspApplication::Create(const info::Application *application_info)
{
	auto application = std::make_shared<RtspApplication>(application_info);
	return application;
}

RtspApplication::RtspApplication(const info::Application *application_info)
	: Application(application_info)
{
}

std::shared_ptr<Stream> RtspApplication::OnCreateStream()
{
	logtd("OnCreateStream");

	auto stream = RtspStream::Create();

	return stream;
}
