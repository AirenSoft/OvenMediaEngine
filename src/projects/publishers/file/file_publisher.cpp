#include "file_publisher.h"

#include <base/ovlibrary/url.h>

#include "file_private.h"

#define UNUSED(expr)  \
	do                \
	{                 \
		(void)(expr); \
	} while (0)

std::shared_ptr<FilePublisher> FilePublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto file = std::make_shared<FilePublisher>(server_config, router);

	if (!file->Start())
	{
		logte("An error occurred while creating FilePublisher");
		return nullptr;
	}

	return file;
}

FilePublisher::FilePublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, router)
{
	logtd("FilePublisher has been create");
}

FilePublisher::~FilePublisher()
{
	logtd("FilePublisher has been terminated finally");
}

bool FilePublisher::Start()
{
	_stop_thread_flag = false;
	_worker_thread = std::thread(&FilePublisher::WorkerThread, this);
	pthread_setname_np(_worker_thread.native_handle(), "FilePubWorker");

	return Publisher::Start();
}

bool FilePublisher::Stop()
{
	_stop_thread_flag = true;

	if (_worker_thread.joinable())
	{
		_worker_thread.join();
	}

	return Publisher::Stop();
}

void FilePublisher::WorkerThread()
{
	ov::StopWatch stat_stop_watch;
	stat_stop_watch.Start();

	while (!_stop_thread_flag)
	{
		if (stat_stop_watch.IsElapsed(500) && stat_stop_watch.Update())
		{
			SessionController();
		}

		usleep(100 * 1000);
	}
}

std::shared_ptr<pub::Application> FilePublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return FileApplication::Create(FilePublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool FilePublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	auto file_application = std::static_pointer_cast<FileApplication>(application);
	if (file_application == nullptr)
	{
		logte("Could not found file application. app:%s", file_application->GetName().CStr());
		return false;
	}

	return true;
}

void FilePublisher::StartSession(std::shared_ptr<FileSession> session)
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

void FilePublisher::StopSession(std::shared_ptr<FileSession> session)
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

void FilePublisher::SplitSession(std::shared_ptr<FileSession> session)
{
	auto record_state = session->GetRecord()->GetState();

	switch (record_state)
	{
		case info::Record::RecordState::Recording:
			session->StopRecord();
			session->StartRecord();
			break;
		default:
			break;
	}
}

void FilePublisher::SessionController()
{
	std::shared_lock<std::shared_mutex> lock(_userdata_sets_mutex);

	auto userdata_sets = _userdata_sets.GetUserdataSets();
	for (auto &[key, userdata] : userdata_sets)
	{
		UNUSED(key);

		// Find a session related to Userdata.
		auto vhost_app_name = info::VHostAppName(userdata->GetVhost(), userdata->GetApplication());
		auto stream = std::static_pointer_cast<FileStream>(GetStream(vhost_app_name, userdata->GetStreamName()));

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
				StartSession(session);
			}

			if (userdata->GetEnable() == false || userdata->GetRemove() == true)
			{
				StopSession(session);
			}

			// When setting interval parameters, perform segmentation recording.
			if ((uint64_t)session->GetRecord()->GetInterval() > 0)
			{
				if (session->GetRecord()->GetRecordTime() > (uint64_t)session->GetRecord()->GetInterval())
				{
					SplitSession(session);
				}
			}
			// When setting schedule parameter, perform segmentation recording.
			else if (session->GetRecord()->GetSchedule().IsEmpty() != true)
			{
				if (session->GetRecord()->IsNextScheduleTimeEmpty() == true)
				{
					if (session->GetRecord()->UpdateNextScheduleTime() == false)
					{
						userdata->SetEnable(false);
						logte("Failed to update next schedule time. reqeuset to stop recording.");
					}
				}
				else if (session->GetRecord()->GetNextScheduleTime() <= std::chrono::system_clock::now())
				{
					SplitSession(session);
					if (session->GetRecord()->UpdateNextScheduleTime() == false)
					{
						userdata->SetEnable(false);
						logte("Failed to update next schedule time. reqeuset to stop recording.");
					}
				}
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

std::shared_ptr<ov::Error> FilePublisher::RecordStart(const info::VHostAppName &vhost_app_name,
													  const std::shared_ptr<info::Record> record)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);

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

		return ov::Error::CreateError(FilePublisherStatusCode::FailureInvalidParameter, error_message);
	}

	// Validation check of duplicate parameters
	if (record->GetSchedule().IsEmpty() == false && record->GetInterval() > 0)
	{
		ov::String error_message = "[Interval] and [Schedule] cannot be used at the same time";

		return ov::Error::CreateError(FilePublisherStatusCode::FailureInvalidParameter, error_message);
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
			return ov::Error::CreateError(FilePublisherStatusCode::FailureInvalidParameter, error_message);
		}

		// Just validation for schedule pattren
		auto match_result = regex.Matches(record->GetSchedule().CStr());
		if (match_result.GetError() != nullptr)
		{
			ov::String error_message = "Invalid [schedule] parameter";
			return ov::Error::CreateError(FilePublisherStatusCode::FailureInvalidParameter, error_message);
		}
	}

	// Checking for the dupilicate id
	if (_userdata_sets.GetByKey(record->GetId()) != nullptr)
	{
		ov::String error_message = "Duplicate ID already exists";

		return ov::Error::CreateError(FilePublisherStatusCode::FailureDupulicateKey, error_message);
	}

	record->SetTransactionId(ov::Random::GenerateString(16));
	record->SetEnable(true);
	record->SetRemove(false);
	record->SetFilePathSetByUser((record->GetFilePath().IsEmpty() != true) ? true : false);
	record->SetInfoPathSetByUser((record->GetInfoPath().IsEmpty() != true) ? true : false);

	_userdata_sets.Set(record->GetId(), record);

	return ov::Error::CreateError(FilePublisherStatusCode::Success, "Success");
}

std::shared_ptr<ov::Error> FilePublisher::RecordStop(const info::VHostAppName &vhost_app_name,
													 const std::shared_ptr<info::Record> record)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);

	if (record->GetId().IsEmpty() == true)
	{
		ov::String error_message = "There is no required parameter [";

		if (record->GetId().IsEmpty() == true)
		{
			error_message += " id";
		}

		error_message += "]";

		return ov::Error::CreateError(FilePublisherStatusCode::FailureInvalidParameter, error_message);
	}

	auto userdata = _userdata_sets.GetByKey(record->GetId());
	if (userdata == nullptr)
	{
		ov::String error_message = ov::String::FormatString("There is no record information related to the ID [%s]", record->GetId().CStr());

		return ov::Error::CreateError(FilePublisherStatusCode::FailureNotExist, error_message);
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

	return ov::Error::CreateError(FilePublisherStatusCode::Success, "Success");
}

std::shared_ptr<ov::Error> FilePublisher::GetRecords(const info::VHostAppName &vhost_app_name,
													 std::vector<std::shared_ptr<info::Record>> &record_list)
{
	std::lock_guard<std::shared_mutex> lock(_userdata_sets_mutex);

	auto userdata_sets = _userdata_sets.GetUserdataSets();
	for (auto &[key, userdata] : userdata_sets)
	{
		UNUSED(key);

		record_list.push_back(userdata);
	}

	return ov::Error::CreateError(FilePublisherStatusCode::Success, "Success");
}
