#include "rtmppush_application.h"

#include "rtmppush_private.h"
#include "rtmppush_stream.h"

std::shared_ptr<RtmpPushApplication> RtmpPushApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<RtmpPushApplication>(publisher, application_info);
	application->Start();
	return application;
}

RtmpPushApplication::RtmpPushApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
	: Application(publisher, application_info)
{
}

RtmpPushApplication::~RtmpPushApplication()
{
	Stop();
	logtd("RtmpPushApplication(%d) has been terminated finally", GetId());
}

bool RtmpPushApplication::Start()
{
	return Application::Start();
}

bool RtmpPushApplication::Stop()
{
	return Application::Stop();
}

std::shared_ptr<pub::Stream> RtmpPushApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count)
{
	logtd("CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return RtmpPushStream::Create(GetSharedPtrAs<pub::Application>(), *info);
}

bool RtmpPushApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	logtd("DeleteStream : %s/%u", info->GetName().CStr(), info->GetId());

	auto stream = std::static_pointer_cast<RtmpPushStream>(GetStream(info->GetId()));
	if (stream == nullptr)
	{
		logte("RtmpPushApplication::Delete stream failed. Cannot find stream (%s)", info->GetName().CStr());
		return false;
	}

	logtd("%s/%s stream has been deleted", GetName().CStr(), stream->GetName().CStr());

	return true;
}
