#include "file_application.h"

#include "file_private.h"
#include "file_publisher.h"
#include "file_stream.h"

#define FILE_PUBLISHER_ERROR_DOMAIN "FilePublisher"

std::shared_ptr<FileApplication> FileApplication::Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
{
	auto application = std::make_shared<FileApplication>(publisher, application_info);
	application->Start();
	return application;
}

FileApplication::FileApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info)
	: Application(publisher, application_info)
{
}

FileApplication::~FileApplication()
{
	Stop();
	logtd("FileApplication(%d) has been terminated finally", GetId());
}

bool FileApplication::Start()
{
	return Application::Start();
}

bool FileApplication::Stop()
{
	return Application::Stop();
}

std::shared_ptr<pub::Stream> FileApplication::CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count)
{
	logtd("Created Stream : %s/%u", info->GetName().CStr(), info->GetId());
	return FileStream::Create(GetSharedPtrAs<pub::Application>(), *info);
}

bool FileApplication::DeleteStream(const std::shared_ptr<info::Stream> &info)
{
	auto stream = std::static_pointer_cast<FileStream>(GetStream(info->GetId()));
	if (stream == nullptr)
	{
		logte("Could not found a stream (%s)", info->GetName().CStr());
		return false;
	}

	logti("File Application %s/%s stream has been deleted", GetName().CStr(), stream->GetName().CStr());

	return true;
}

void FileApplication::SessionStart(std::shared_ptr<FileSession> session)
{
	// Check the status of the session.
	auto session_state = session->GetState();

	switch (session_state)
	{
		// State of disconnected and ready to connect
		case pub::Session::SessionState::Ready:
			session->Start();
			break;
		case pub::Session::SessionState::Stopped:
			session->Start();
			break;
		// State of Recording
		case pub::Session::SessionState::Started:
			[[fallthrough]];
		// State of Stopping
		case pub::Session::SessionState::Stopping:
			[[fallthrough]];
		// State of Record failed
		case pub::Session::SessionState::Error:
			[[fallthrough]];
		default:
			break;
	}

	auto next_session_state = session->GetState();
	if (session_state != next_session_state)
	{
		logtd("Changed State. State(%d - %d)", session_state, next_session_state);
	}
}

void FileApplication::SessionStop(std::shared_ptr<FileSession> session)
{
	auto session_state = session->GetState();

	switch (session_state)
	{
		case pub::Session::SessionState::Started:
			session->Stop();
			break;
		default:
			break;
	}

	auto next_session_state = session->GetState();
	if (session_state != next_session_state)
	{
		logtd("Changed State. State(%d - %d)", session_state, next_session_state);
	}
}

void FileApplication::SessionUpdate(std::shared_ptr<FileStream> stream, std::shared_ptr<info::Record> userdata)
{
	// If there is no session, create a new file(record) session.
	auto session = std::static_pointer_cast<FileSession>(stream->GetSession(userdata->GetSessionId()));
	if (session == nullptr)
	{
		session = stream->CreateSession();
		if (session == nullptr)
		{
			logte("Could not create session");
			return;
		}
		userdata->SetSessionId(session->GetId());

		session->SetRecord(userdata);
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

void FileApplication::SessionUpdateByStream(std::shared_ptr<FileStream> stream, bool stopped)
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
			userdata->SetState(info::Record::RecordState::Ready);
		}
		else
		{
			SessionUpdate(stream, userdata);
		}
	}
}

void FileApplication::SessionUpdateByUser()
{
	for (uint32_t i = 0; i < _userdata_sets.GetCount(); i++)
	{
		auto userdata = _userdata_sets.GetAt(i);
		if (userdata == nullptr)
			continue;

		// Find a stream related to Userdata.
		auto stream = std::static_pointer_cast<FileStream>(GetStream(userdata->GetStreamName()));
		if (stream != nullptr && stream->GetState() == pub::Stream::State::STARTED)
		{
			SessionUpdate(stream, userdata);
		}
		else
		{
			userdata->SetState(info::Record::RecordState::Ready);
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

#if 0
void FileApplication::SessionController()
{
	for (uint32_t i = 0; i < _userdata_sets.GetCount(); i++)
	{
		auto userdata = _userdata_sets.GetAt(i);
		if (userdata == nullptr)
			continue;

		// Find a session related to Userdata.
		auto stream = std::static_pointer_cast<FileStream>(GetStream(userdata->GetStreamName()));

		if (stream != nullptr && stream->GetState() == pub::Stream::State::STARTED)
		{
			// If there is no session, create a new file(record) session.
			auto session = std::static_pointer_cast<FileSession>(stream->GetSession(userdata->GetSessionId()));
			if (session == nullptr)
			{
				session = stream->CreateSession();
				if (session == nullptr)
				{
					logte("Could not create session");
					continue;
				}
				userdata->SetSessionId(session->GetId());

				session->SetRecord(userdata);
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
		else
		{
			userdata->SetState(info::Record::RecordState::Ready);
		}

		// Garbage collector of removed userdata sets
		if (userdata->GetRemove() == true)
		{
			logtd("Remove userdata of file publiser. id(%s)", userdata->GetId().CStr());

			if (stream != nullptr && userdata->GetSessionId() != 0)
				stream->DeleteSession(userdata->GetSessionId());

			_userdata_sets.DeleteByKey(userdata->GetId());
		}
	}
}
#endif

std::shared_ptr<ov::Error> FileApplication::RecordStart(const std::shared_ptr<info::Record> record)
{
	// Checking for the required parameters
	if (record->GetId().IsEmpty() == true || record->GetStreamName().IsEmpty() == true)
	{
		ov::String error_message = "There is no required parameter [";

		if (record->GetId().IsEmpty() == true)
		{
			error_message += " id";
		}

		if (record->GetStreamName().IsEmpty() == true)
		{
			error_message += " stream.name";
		}

		error_message += "]";

		return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureInvalidParameter, error_message);
	}

	// Validation check of duplicate parameters
	if (record->GetSchedule().IsEmpty() == false && record->GetInterval() > 0)
	{
		ov::String error_message = "[Interval] and [Schedule] cannot be used at the same time";

		return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureInvalidParameter, error_message);
	}

	// Validation check of schedule Parameter
	if (record->GetSchedule().IsEmpty() == false)
	{
		ov::String pattern = R"(^(\*|([0-9]|1[0-9]|2[0-9]|3[0-9]|4[0-9]|5[0-9])|\*\/([0-9]|1[0-9]|2[0-9]|3[0-9]|4[0-9]|5[0-9])) (\*|([0-9]|1[0-9]|2[0-9]|3[0-9]|4[0-9]|5[0-9])|\*\/([0-9]|1[0-9]|2[0-9]|3[0-9]|4[0-9]|5[0-9])) (\*|([0-9]|1[0-9]|2[0-3])|\*\/([0-9]|1[0-9]|2[0-3]))$)";
		auto regex = ov::Regex(pattern);
		auto error = regex.Compile();

		if (error != nullptr)
		{
			ov::String error_message = "Invalid regular expression pattern";
			return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureInvalidParameter, error_message);
		}

		// Just validation for schedule pattren
		auto match_result = regex.Matches(record->GetSchedule().CStr());
		if (match_result.GetError() != nullptr)
		{
			ov::String error_message = "Invalid [schedule] parameter";
			return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureInvalidParameter, error_message);
		}
	}

	// Checking for the dupilicate id
	if (_userdata_sets.GetByKey(record->GetId()) != nullptr)
	{
		ov::String error_message = "Duplicate ID already exists";

		return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureDupulicateKey, error_message);
	}

	record->SetTransactionId(ov::Random::GenerateString(16));
	record->SetEnable(true);
	record->SetRemove(false);
	record->SetFilePathSetByUser((record->GetFilePath().IsEmpty() != true) ? true : false);
	record->SetInfoPathSetByUser((record->GetInfoPath().IsEmpty() != true) ? true : false);

	_userdata_sets.Set(record->GetId(), record);

	SessionUpdateByUser();

	return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::Success, "Success");
}

std::shared_ptr<ov::Error> FileApplication::RecordStop(const std::shared_ptr<info::Record> record)
{
	if (record->GetId().IsEmpty() == true)
	{
		ov::String error_message = "There is no required parameter [";

		if (record->GetId().IsEmpty() == true)
		{
			error_message += " id";
		}

		error_message += "]";

		return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureInvalidParameter, error_message);
	}

	auto userdata = _userdata_sets.GetByKey(record->GetId());
	if (userdata == nullptr)
	{
		ov::String error_message = ov::String::FormatString("There is no record information related to the ID [%s]", record->GetId().CStr());

		return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureNotExist, error_message);
	}

	userdata->SetEnable(false);
	userdata->SetRemove(true);

	// Copy current recording information to the requested parameters.
	//
	record->SetState(info::Record::RecordState::Stopping);
	record->SetMetadata(userdata->GetMetadata());
	record->SetStream(userdata->GetStream());
	record->SetSessionId(userdata->GetSessionId());
	record->SetInterval(userdata->GetInterval());
	record->SetSchedule(userdata->GetSchedule());
	record->SetSegmentationRule(userdata->GetSegmentationRule());
	record->SetFilePath(userdata->GetFilePath());
	record->SetInfoPath(userdata->GetInfoPath());
	record->SetTmpPath(userdata->GetTmpPath());
	record->SetRecordBytes(userdata->GetRecordBytes());
	record->SetRecordTotalBytes(userdata->GetRecordTotalBytes());
	record->SetRecordTime(userdata->GetRecordTime());
	record->SetRecordTotalTime(userdata->GetRecordTotalTime());
	record->SetSqeuence(userdata->GetSequence());
	record->SetCreatedTime(userdata->GetCreatedTime());
	record->SetRecordStartTime(userdata->GetRecordStartTime());
	record->SetRecordStopTime(userdata->GetRecordStopTime());

	SessionUpdateByUser();

	return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::Success, "Success");
}

std::shared_ptr<ov::Error> FileApplication::GetRecords(const std::shared_ptr<info::Record> record, std::vector<std::shared_ptr<info::Record>> &results)
{
	for (uint32_t i = 0; i < _userdata_sets.GetCount(); i++)
	{
		auto userdata = _userdata_sets.GetAt(i);
		if (userdata == nullptr)
			continue;

		if (!record->GetId().IsEmpty() && record->GetId() != userdata->GetId())
			continue;

		results.push_back(userdata);
	}

	return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::Success, "Success");
}
