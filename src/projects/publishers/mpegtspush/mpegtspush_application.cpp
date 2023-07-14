#include "mpegtspush_application.h"

#include "mpegtspush_private.h"
#include "mpegtspush_publisher.h"
#include "mpegtspush_stream.h"

std::shared_ptr<MpegtsPushApplication> MpegtsPushApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<MpegtsPushApplication>(publisher, application_info);
	application->Start();
	return application;
}

MpegtsPushApplication::MpegtsPushApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
	: PushApplication(publisher, application_info)
{
}

MpegtsPushApplication::~MpegtsPushApplication()
{
	Stop();
	logtd("MpegtsPushApplication(%d) has been terminated finally", GetId());
}

bool MpegtsPushApplication::Start()
{
	return PushApplication::Start();
}

bool MpegtsPushApplication::Stop()
{
	return PushApplication::Stop();
}

std::shared_ptr<pub::Stream> MpegtsPushApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count)
{
	logtd("CreateStream : %s/%u", info->GetName().CStr(), info->GetId());

	return MpegtsPushStream::Create(GetSharedPtrAs<pub::Application>(), *info);
}

bool MpegtsPushApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	logtd("DeleteStream : %s/%u", info->GetName().CStr(), info->GetId());

	auto stream = std::static_pointer_cast<MpegtsPushStream>(GetStream(info->GetId()));
	if (stream == nullptr)
	{
		logte("MpegtsPushApplication::Delete stream failed. Cannot find stream (%s)", info->GetName().CStr());
		return false;
	}

	logtd("%s/%s stream has been deleted", GetName().CStr(), stream->GetName().CStr());

	return true;
}

std::shared_ptr<ov::Error> MpegtsPushApplication::StartPush(const std::shared_ptr<info::Push> &push)
{
	// Validation check for required parameters
	if (push->GetId().IsEmpty() == true ||
		push->GetStreamName().IsEmpty() == true ||
		push->GetUrl().IsEmpty() == true ||
		push->GetProtocol().IsEmpty() == true)
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
	if (push->GetUrl().HasPrefix("udp://") == false &&
		push->GetUrl().HasPrefix("tcp://") == false)
	{
		ov::String error_message = "Unsupported protocol";
		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	// Remove suffix '/" of mpegts url
	while (push->GetUrl().HasSuffix("/"))
	{
		auto index_of { push->GetUrl().IndexOfRev('/') };
		if (index_of >= 0)
		{
			ov::String tmp_url = push->GetUrl().Substring(0, static_cast<size_t>(index_of));
			push->SetUrl(tmp_url);
		}
	}

	// Validation check only push not tcp listen
	if (push->GetUrl().HasPrefix("tcp://") &&
		push->GetUrl().HasSuffix("listen"))
	{
		ov::String error_message = "Unsupported tcp listen";
		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	return PushApplication::StartPush(push);
}
