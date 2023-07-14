#include "rtmppush_application.h"

#include "rtmppush_private.h"
#include "rtmppush_publisher.h"
#include "rtmppush_stream.h"

std::shared_ptr<RtmpPushApplication> RtmpPushApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<RtmpPushApplication>(publisher, application_info);
	application->Start();
	return application;
}

RtmpPushApplication::RtmpPushApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
	: PushApplication(publisher, application_info)
{
}

RtmpPushApplication::~RtmpPushApplication()
{
	Stop();
	logtd("RtmpPushApplication(%d) has been terminated finally", GetId());
}

bool RtmpPushApplication::Start()
{
	return PushApplication::Start();
}

bool RtmpPushApplication::Stop()
{
	return PushApplication::Stop();
}


std::shared_ptr<pub::Stream> RtmpPushApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count)
{
	return RtmpPushStream::Create(GetSharedPtrAs<pub::Application>(), *info);
}


bool RtmpPushApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	auto stream = std::static_pointer_cast<RtmpPushStream>(GetStream(info->GetId()));
	if (stream == nullptr)
	{
		logte("RtmpPushApplication::Delete stream failed. Cannot find stream (%s)", info->GetName().CStr());
		return false;
	}

	logtd("%s/%s stream has been deleted", GetName().CStr(), stream->GetName().CStr());

	return true;
}

std::shared_ptr<ov::Error> RtmpPushApplication::StartPush(const std::shared_ptr<info::Push> &push)
{
	// Validation check for required parameters
	if (push->GetId().IsEmpty() == true || push->GetStreamName().IsEmpty() == true || push->GetUrl().IsEmpty() == true || push->GetProtocol().IsEmpty() == true)
	{
		ov::String error_message = "There is no required parameter [";

		if (push->GetId().IsEmpty() == true)
		{
			error_message += " id";
		}

		if (push->GetStreamName().IsEmpty() == true)
		{
			error_message += " stream.name";
		}

		if (push->GetUrl().IsEmpty() == true)
		{
			error_message += " url";
		}

		if (push->GetProtocol().IsEmpty() == true)
		{
			error_message += " protocol";
		}

		error_message += "]";

		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	// Validation check for protocol scheme
	if (push->GetUrl().HasPrefix("rtmp://") == false && push->GetUrl().HasPrefix("rtmps://") == false)
	{
		ov::String error_message = "Unsupported protocol";

		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	// Remove suffix '/" of rtmp url
	while (push->GetUrl().HasSuffix("/"))
	{
		ov::String tmp_url = push->GetUrl().Substring(0, push->GetUrl().IndexOfRev('/'));
		push->SetUrl(tmp_url);
	}

	return PushApplication::StartPush(push);
}
