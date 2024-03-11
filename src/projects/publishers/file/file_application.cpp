#include "file_application.h"

#include "file_private.h"
#include "file_publisher.h"
#include "file_stream.h"

#define FILE_PUBLISHER_ERROR_DOMAIN "FilePublisher"

namespace pub
{
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

		auto config = GetConfig();
		auto file_conifg = config.GetPublishers().GetFilePublisher();
		auto record_config = file_conifg.GetRecord();

		if (record_config.IsEnabled() == true)
		{
			auto records_info = GetRecordInfoFromFile(record_config.GetRecordInfo(), info);
			for (auto record : records_info)
			{
				auto result = RecordStart(record);
				if (result->GetCode() != FilePublisher::FilePublisherStatusCode::Success)
				{
					logtw("FileStream(%s/%s) - Failed to start record(%s) status(%d) description(%s)", GetName().CStr(), info->GetName().CStr(), record->GetId().CStr(), result->GetCode(), result->GetMessage().CStr());
				}
			}
		}

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

		// Removes only automatically recorded information with the same stream name.
		auto record_info_list = _record_info_list.GetByStreamName(info->GetName());
		for (auto record_info : record_info_list)
		{
			if (record_info->IsAutomated() == false)
			{
				continue;
			}

			auto result = RecordStop(record_info);
			if (result->GetCode() != FilePublisher::FilePublisherStatusCode::Success)
			{
				logtw("FileStream(%s/%s) - Failed to start record(%s) status(%d) description(%s)", GetName().CStr(), info->GetName().CStr(), record_info->GetId().CStr(), result->GetCode(), result->GetMessage().CStr());
			}
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
			logti("Recording Started. %s", userdata->GetInfoString().CStr());
		}

		if (userdata->GetEnable() == false || userdata->GetRemove() == true)
		{
			SessionStop(session);
			logti("Recording Completed. %s", userdata->GetInfoString().CStr());

		}
	}

	void FileApplication::SessionUpdateByStream(std::shared_ptr<FileStream> stream, bool stopped)
	{
		if (stream == nullptr)
		{
			return;
		}

		for (uint32_t i = 0; i < _record_info_list.GetCount(); i++)
		{
			auto userdata = _record_info_list.GetAt(i);
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
		for (uint32_t i = 0; i < _record_info_list.GetCount(); i++)
		{
			auto userdata = _record_info_list.GetAt(i);
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

				_record_info_list.DeleteByKey(userdata->GetId());
			}
		}
	}

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

			// Just validation for schedule pattern
			auto match_result = regex.Matches(record->GetSchedule().CStr());
			if (match_result.GetError() != nullptr)
			{
				ov::String error_message = "Invalid [schedule] parameter";
				return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureInvalidParameter, error_message);
			}
		}

		// Checking for the duplicate id
		if (_record_info_list.GetByKey(record->GetId()) != nullptr)
		{
			ov::String error_message = "Duplicate ID already exists";

			return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureDuplicateKey, error_message);
		}

		record->SetTransactionId(ov::Random::GenerateString(16));
		record->SetEnable(true);
		record->SetRemove(false);
		record->SetFilePathSetByUser((record->GetFilePath().IsEmpty() != true) ? true : false);
		record->SetInfoPathSetByUser((record->GetInfoPath().IsEmpty() != true) ? true : false);

		_record_info_list.Set(record->GetId(), record);

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

		auto record_info = _record_info_list.GetByKey(record->GetId());
		if (record_info == nullptr)
		{
			ov::String error_message = ov::String::FormatString("There is no record information related to the ID [%s]", record->GetId().CStr());

			return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::FailureNotExist, error_message);
		}

		record_info->SetEnable(false);
		record_info->SetRemove(true);

		// Copy current recording information to the requested parameters.
		//
		record->SetState(info::Record::RecordState::Stopping);
		record->SetMetadata(record_info->GetMetadata());
		record->SetStreamName(record_info->GetStreamName());
		record->SetTrackIds(record_info->GetTrackIds());
		record->SetVariantNames(record_info->GetVariantNames());
		record->SetSessionId(record_info->GetSessionId());
		record->SetInterval(record_info->GetInterval());
		record->SetSchedule(record_info->GetSchedule());
		record->SetSegmentationRule(record_info->GetSegmentationRule());
		record->SetFilePath(record_info->GetFilePath());
		record->SetInfoPath(record_info->GetInfoPath());
		record->SetTmpPath(record_info->GetTmpPath());
		record->SetRecordBytes(record_info->GetRecordBytes());
		record->SetRecordTotalBytes(record_info->GetRecordTotalBytes());
		record->SetRecordTime(record_info->GetRecordTime());
		record->SetRecordTotalTime(record_info->GetRecordTotalTime());
		record->SetSequence(record_info->GetSequence());
		record->SetCreatedTime(record_info->GetCreatedTime());
		record->SetRecordStartTime(record_info->GetRecordStartTime());
		record->SetRecordStopTime(record_info->GetRecordStopTime());

		SessionUpdateByUser();

		return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::Success, "Success");
	}

	std::shared_ptr<ov::Error> FileApplication::GetRecords(const std::shared_ptr<info::Record> record_query, std::vector<std::shared_ptr<info::Record>> &results)
	{
		for (uint32_t i = 0; i < _record_info_list.GetCount(); i++)
		{
			auto record_info = _record_info_list.GetAt(i);
			if (record_info == nullptr)
				continue;

			if (!record_query->GetId().IsEmpty() && record_query->GetId() != record_info->GetId())
				continue;

			results.push_back(record_info);
		}

		return ov::Error::CreateError(FILE_PUBLISHER_ERROR_DOMAIN, FilePublisher::FilePublisherStatusCode::Success, "Success");
	}

	std::vector<std::shared_ptr<info::Record>> FileApplication::GetRecordInfoFromFile(const ov::String &file_path, const std::shared_ptr<info::Stream> &stream_info)
	{
		std::vector<std::shared_ptr<info::Record>> results;

		ov::String final_path = ov::GetFilePath(file_path, cfg::ConfigManager::GetInstance()->GetConfigPath());

		pugi::xml_document xml_doc;
		auto load_result = xml_doc.load_file(final_path.CStr());
		if (load_result == false)
		{
			logte("FileStream(%s/%s) - Failed to load Record info file(%s) status(%d) description(%s)", GetName().CStr(), stream_info->GetName().CStr(), final_path.CStr(), load_result.status, load_result.description());
			return results;
		}

		auto root_node = xml_doc.child("RecordInfo");
		if (root_node.empty())
		{
			logte("FileStream(%s/%s) - Failed to load Record info file(%s) because root node is not found", GetName().CStr(), stream_info->GetName().CStr(), final_path.CStr());
			return results;
		}

		for (pugi::xml_node record_node = root_node.child("Record"); record_node; record_node = record_node.next_sibling("Record"))
		{
			bool enable = (strcmp(record_node.child_value("Enable"), "true") == 0) ? true : false;
			ov::String target_stream_name = record_node.child_value("StreamName");
			ov::String file_path = record_node.child_value("FilePath");
			ov::String info_path = record_node.child_value("InfoPath");
			ov::String variant_names = record_node.child_value("VariantNames");
			ov::String segment_interval = record_node.child_value("SegmentInterval");
			ov::String segment_schedule = record_node.child_value("SegmentSchedule");
			ov::String segment_rule = record_node.child_value("SegmentRule");
			ov::String metadata = record_node.child_value("Metadata");

			if(enable == false)
			{
				continue;
			}

			// stream_name can be regex
			ov::Regex _target_stream_name_regex = ov::Regex::CompiledRegex(ov::Regex::WildCardRegex(target_stream_name));
			auto match_result = _target_stream_name_regex.Matches(stream_info->GetName().CStr());

			if (!match_result.IsMatched())
			{
				continue;
			}

			auto record = std::make_shared<info::Record>();
			if(record == nullptr)
			{
				continue;
			}

			record->SetId(ov::Random::GenerateString(16));
			record->SetEnable(enable);
			record->SetVhost(GetName().GetVHostName());
			record->SetApplication(GetName().GetAppName());

			record->SetStreamName(stream_info->GetName().CStr());
			if(variant_names.IsEmpty() == false)
			{
				auto variant_name_list = ov::String::Split(variant_names.Trim(), ",");
				for(auto variant_name : variant_name_list)
				{
					record->AddVariantName(variant_name);
				}
			}

			record->SetFilePath(file_path);
			record->SetInfoPath(info_path);
			record->SetAutomated(true);
			record->SetInterval(ov::Converter::ToInt32(segment_interval));
			record->SetSegmentationRule(segment_rule);
			record->SetSchedule(segment_schedule);
			record->SetMetadata(metadata);

			results.push_back(record);
		}

		return results;
	}
}  // namespace pub
