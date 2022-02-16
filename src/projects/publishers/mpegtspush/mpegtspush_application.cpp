#include "mpegtspush_application.h"

#include "mpegtspush_private.h"
#include "mpegtspush_publisher.h"
#include "mpegtspush_stream.h"

#define MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN					"MPEGTSPushPublisher"

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
	return Application::Start();
}

bool MpegtsPushApplication::Stop()
{
	return Application::Stop();
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

void MpegtsPushApplication::SessionUpdate(std::shared_ptr<MpegtsPushStream> stream, std::shared_ptr<info::Push> userdata)
{
	// If there is no session, create a new file(record) session.
	auto session = std::static_pointer_cast<MpegtsPushSession>(stream->GetSession(userdata->GetSessionId()));
	if (session == nullptr)
	{
		session = stream->CreateSession();
		if (session == nullptr)
		{
			logte("Could not create session");
			return;
		}
		userdata->SetSessionId(session->GetId());

		session->SetPush(userdata);
	}

	if (userdata->GetEnable() == true && userdata->GetRemove() == false)
	{
		SessionStart(session);
	}

	if (userdata->GetEnable() == false || userdata->GetRemove() == true)
	{
		SessionStop(session);
	}
}

void MpegtsPushApplication::SessionUpdateByStream(std::shared_ptr<MpegtsPushStream> stream, bool stopped)
{
	if (stream == nullptr)
	{
		return;
	}

	for (uint32_t i = 0; i < _userdata_sets.GetCount(); i++)
	{
		auto userdata = _userdata_sets.GetAt(i);
		if (userdata == nullptr)
			continue;

		if (userdata->GetStreamName() != stream->GetName())
			continue;

		if (stopped == true)
		{
			userdata->SetState(info::Push::PushState::Ready);
		}
		else
		{
			SessionUpdate(stream, userdata);
		}
	}
}

void MpegtsPushApplication::SessionUpdateByUser()
{
	for (uint32_t i = 0; i < _userdata_sets.GetCount(); i++)
	{
		auto userdata = _userdata_sets.GetAt(i);
		if (userdata == nullptr)
			continue;

		// Find a stream related to Userdata.
		auto stream = std::static_pointer_cast<MpegtsPushStream>(GetStream(userdata->GetStreamName()));
		if (stream != nullptr && stream->GetState() == pub::Stream::State::STARTED)
		{
			SessionUpdate(stream, userdata);
		}
		else
		{
			userdata->SetState(info::Push::PushState::Ready);
		}

		if (userdata->GetRemove() == true)
		{
			if (stream != nullptr && userdata->GetSessionId() != 0)
			{
				stream->DeleteSession(userdata->GetSessionId());
			}

			_userdata_sets.DeleteByKey(userdata->GetId());
		}
	}
}

void MpegtsPushApplication::SessionStart(std::shared_ptr<MpegtsPushSession> session)
{
	// Check the status of the session.
	auto session_state = session->GetState();

	switch (session_state)
	{
		// State of disconnected and ready to connect
		case pub::Session::SessionState::Ready:
			[[fallthrough]];
		// State of stopped
		case pub::Session::SessionState::Stopped:
			[[fallthrough]];
		// State of failed (connection refused, disconnected)
		case pub::Session::SessionState::Error:
			session->Start();
			break;
		// State of Started
		case pub::Session::SessionState::Started:
			[[fallthrough]];
		// State of Stopping
		case pub::Session::SessionState::Stopping:
			break;
	}

	auto next_session_state = session->GetState();
	if (session_state != next_session_state)
	{
		logtd("Changed State. State(%d - %d)", session_state, next_session_state);
	}
}

void MpegtsPushApplication::SessionStop(std::shared_ptr<MpegtsPushSession> session)
{
	auto session_state = session->GetState();

	switch (session_state)
	{
		case pub::Session::SessionState::Started:
			session->Stop();
			break;
		case pub::Session::SessionState::Ready:
			[[fallthrough]];
		case pub::Session::SessionState::Stopping:
			[[fallthrough]];
		case pub::Session::SessionState::Stopped:
			[[fallthrough]];
		case pub::Session::SessionState::Error:
			break;
	}

	auto next_session_state = session->GetState();
	if (session_state != next_session_state)
	{
		logtd("Changed State. State(%d - %d)", session_state, next_session_state);
	}
}

std::shared_ptr<ov::Error> MpegtsPushApplication::PushStart(const std::shared_ptr<info::Push> &push)
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
		return ov::Error::CreateError(MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	// Validation check for dupulicate id
	if (_userdata_sets.GetByKey(push->GetId()) != nullptr)
	{
		ov::String error_message = "Duplicate ID already exists";
		return ov::Error::CreateError(MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureDupulicateKey, error_message);
	}

	// Validation check for protocol scheme
	if (push->GetUrl().HasPrefix("udp://") == false &&
		push->GetUrl().HasPrefix("tcp://") == false)
	{
		ov::String error_message = "Unsupported protocol";
		return ov::Error::CreateError(MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
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
		return ov::Error::CreateError(MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	push->SetEnable(true);
	push->SetRemove(false);
	_userdata_sets.Set(push);

	SessionUpdateByUser();
	return ov::Error::CreateError(MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::Success, "Success");
}

std::shared_ptr<ov::Error> MpegtsPushApplication::PushStop(const std::shared_ptr<info::Push> &push)
{
	if (push->GetId().IsEmpty() == true)
	{
		ov::String error_message = "There is no required parameter [";

		if (push->GetId().IsEmpty() == true)
		{
			error_message += " id";
		}

		error_message += "]";

		return ov::Error::CreateError(MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
	}

	auto userdata = _userdata_sets.GetByKey(push->GetId());
	if (userdata == nullptr)
	{
		ov::String error_message = ov::String::FormatString("There is no push information related to the ID [%s]", push->GetId().CStr());

		return ov::Error::CreateError(MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureNotExist, error_message);
	}

	userdata->SetEnable(false);
	userdata->SetRemove(true);

	SessionUpdateByUser();

	return ov::Error::CreateError(MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::Success, "Success");
}

std::shared_ptr<ov::Error> MpegtsPushApplication::GetPushes(const std::shared_ptr<info::Push> push, std::vector<std::shared_ptr<info::Push>> &results)
{
	for (uint32_t i = 0; i < _userdata_sets.GetCount(); i++)
	{
		auto userdata = _userdata_sets.GetAt(i);
		if (userdata == nullptr)
			continue;

		if (!push->GetId().IsEmpty() && push->GetId() != userdata->GetId())
			continue;

		results.push_back(userdata);
	}

	return ov::Error::CreateError(MPEG_TS_PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::Success, "Success");
}
