#include "file_session.h"

#include <base/info/application.h>
#include <base/info/host.h>
#include <base/info/stream.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/publisher/application.h>
#include <base/publisher/stream.h>
#include <config/config.h>

#include <regex>

#include "file_export.h"
#include "file_private.h"

std::shared_ptr<FileSession> FileSession::Create(const std::shared_ptr<pub::Application> &application,
												 const std::shared_ptr<pub::Stream> &stream,
												 uint32_t session_id)
{
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
	auto session = std::make_shared<FileSession>(session_info, application, stream);

	return session;
}

FileSession::FileSession(const info::Session &session_info,
						 const std::shared_ptr<pub::Application> &application,
						 const std::shared_ptr<pub::Stream> &stream)
	: pub::Session(session_info, application, stream),
	  _writer(nullptr)
{
}

FileSession::~FileSession()
{
	logtd("FileSession(%d) has been terminated finally", GetId());
}

bool FileSession::Start()
{
	std::lock_guard<std::shared_mutex> mlock(_lock);

	if (StartRecord() == false)
	{
		logte("Failed to start recording. id(%d)", GetId());

		return false;
	}

	logtd("FileSession(%d) has started.", GetId());

	return Session::Start();
}

bool FileSession::Stop()
{
	std::lock_guard<std::shared_mutex> mlock(_lock);

	if (StopRecord() == false)
	{
		logte("Failed to stop recording. id(%d)", GetId());

		return false;
	}

	logtd("FileSession(%d) has stoped", GetId());

	return Session::Stop();
}

bool FileSession::Split()
{
	if (StopRecord() == false)
	{
		logte("Failed to stop recording. id(%d)", GetId());
		return false;
	}

	if (StartRecord() == false)
	{
		logte("Failed to start recording. id(%d)", GetId());
		return false;
	}

	return true;
}

bool FileSession::StartRecord()
{
	GetRecord()->UpdateRecordStartTime();
	GetRecord()->SetOutputFilePath(GetOutputFilePath());
	GetRecord()->SetOutputInfoPath(GetOutputFileInfoPath());
	GetRecord()->SetTmpPath(GetOutputTempFilePath(GetRecord()));
	GetRecord()->SetState(info::Record::RecordState::Recording);

	// Get extension and container format
	ov::String output_extention = ov::PathManager::ExtractExtension(GetRecord()->GetOutputFilePath());
	ov::String output_format = FileWriter::GetFormatByExtension(output_extention, "mpegts");

	logtd("Recording file information. seq(%d), extention(%s), format(%s), filePath(%s), tmpPath(%s), infoPath(%s)",
		  GetRecord()->GetSequence(),
		  output_extention.CStr(),
		  output_format.CStr(),
		  GetRecord()->GetOutputFilePath().CStr(),
		  GetRecord()->GetTmpPath().CStr(),
		  GetRecord()->GetOutputInfoPath().CStr());

	// Create directory for temporary file
	ov::String tmp_directory = ov::PathManager::ExtractPath(GetRecord()->GetTmpPath());
	ov::String tmp_real_directory = ov::PathManager::Combine(GetRootPath(), tmp_directory);

	if (MakeDirectoryRecursive(tmp_real_directory.CStr()) == false)
	{
		logte("Could not create directory. path(%s)", tmp_real_directory.CStr());

		SetState(SessionState::Error);
		GetRecord()->SetState(info::Record::RecordState::Error);

		return false;
	}

	// logtd("The temporary directory was created successfully. (%s)", tmp_real_directory.CStr());

	_writer = FileWriter::Create();
	if (_writer == nullptr)
	{
		SetState(SessionState::Error);
		GetRecord()->SetState(info::Record::RecordState::Error);

		return false;
	}

	if (_writer->SetPath(ov::PathManager::Combine(GetRootPath(), GetRecord()->GetTmpPath()), output_format) == false)
	{
		SetState(SessionState::Error);
		GetRecord()->SetState(info::Record::RecordState::Error);

		_writer = nullptr;

		return false;
	}

	// The mode to specify the initial value of the timestamp stored in the file to zero,
	// or keep it at the same value as the source timestamp
	if (GetRecord()->GetSegmentationRule() == "continuity")
	{
		_writer->SetTimestampRecalcMode(FileWriter::TIMESTAMP_PASSTHROUGH_MODE);
	}
	else if (GetRecord()->GetSegmentationRule() == "discontinuity")
	{
		_writer->SetTimestampRecalcMode(FileWriter::TIMESTAMP_STARTZERO_MODE);
	}

	logtd("Create temporary file(%s)", _writer->GetPath().CStr());

	for (auto &track_item : GetStream()->GetTracks())
	{
		auto &track = track_item.second;

		// If the selected track list exists. if the current trackid does not exist on the list, ignore it.
		// If no track list is selected, save all tracks.
		if ((selected_tracks.empty() != true) &&
			(std::find(selected_tracks.begin(), selected_tracks.end(), track->GetId()) == selected_tracks.end()))
		{
			continue;
		}

		if (FileWriter::IsSupportCodec(output_format, track->GetCodecId()) == false)
		{
			logtw("%s format does not support the codec(%d)", output_format.CStr(), track->GetCodecId());
			continue;
		}

		auto track_info = FileTrackInfo::Create();

		track_info->SetCodecId(track->GetCodecId());
		track_info->SetBitrate(track->GetBitrate());
		track_info->SetTimeBase(track->GetTimeBase());
		track_info->SetWidth(track->GetWidth());
		track_info->SetHeight(track->GetHeight());
		track_info->SetSample(track->GetSample());
		track_info->SetChannel(track->GetChannel());
		track_info->SetExtradata(track->GetCodecExtradata());

		bool ret = _writer->AddTrack(track->GetMediaType(), track->GetId(), track_info);
		if (ret == false)
		{
			logtw("Failed to add media track");
		}
	}

	if (_writer->Start() == false)
	{
		_writer = nullptr;
		SetState(SessionState::Error);
		GetRecord()->SetState(info::Record::RecordState::Error);

		return false;
	}

	logtd("Recording started. id: %d", GetId());

	return true;
}

bool FileSession::StopRecord()
{
	if (_writer != nullptr)
	{
		_writer->Stop();

		SetState(SessionState::Stopping);

		GetRecord()->SetState(info::Record::RecordState::Stopping);

		GetRecord()->UpdateRecordStopTime();

		GetRecord()->SetOutputFilePath(GetOutputFilePath());

		GetRecord()->SetOutputInfoPath(GetOutputFileInfoPath());

		// Create directory for recorded file
		ov::String output_path = ov::PathManager::Combine(GetRootPath(), GetRecord()->GetOutputFilePath());
		ov::String output_directory = ov::PathManager::ExtractPath(output_path);

		if (MakeDirectoryRecursive(output_directory.CStr()) == false)
		{
			logte("Could not create directory. path: %s", output_directory.CStr());

			SetState(SessionState::Error);
			GetRecord()->SetState(info::Record::RecordState::Error);

			return false;
		}

		// Create directory for information file
		ov::String info_path = ov::PathManager::Combine(GetRootPath(), GetRecord()->GetOutputInfoPath());
		ov::String info_directory = ov::PathManager::ExtractPath(info_path);

		if (MakeDirectoryRecursive(info_directory.CStr()) == false)
		{
			logte("Could not create directory. path: %s", info_directory.CStr());

			SetState(SessionState::Error);
			GetRecord()->SetState(info::Record::RecordState::Error);

			return false;
		}

		// Moves temporary files to a user-defined path.
		ov::String tmp_output_path = _writer->GetPath();

		if (rename(tmp_output_path.CStr(), output_path.CStr()) != 0)
		{
			logte("Failed to move file. from: %s to: %s", tmp_output_path.CStr(), output_path.CStr());

			SetState(SessionState::Error);
			GetRecord()->SetState(info::Record::RecordState::Error);

			return false;
		}

		logtd("Replace the temporary file name with the target file name. from: %s, to: %s", tmp_output_path.CStr(), output_path.CStr());

		// Append recorded information to the information file
		if (FileExport::GetInstance()->ExportRecordToXml(info_path, GetRecord()) == false)
		{
			logte("Failed to export xml file. path: %s", info_path.CStr());
		}

		logtd("Appends the recording result to the information file. path: %s", info_path.CStr());

		GetRecord()->SetState(info::Record::RecordState::Stopped);
		GetRecord()->IncreaseSequence();

		_writer = nullptr;

		logtd("Recording finished. id: %d", GetId());
	}

	return true;
}

bool FileSession::SendOutgoingData(const std::any &packet)
{
	std::lock_guard<std::shared_mutex> mlock(_lock);

	std::shared_ptr<MediaPacket> session_packet;

	try
	{
		session_packet = std::any_cast<std::shared_ptr<MediaPacket>>(packet);
		if (session_packet == nullptr)
		{
			return false;
		}
	}
	catch (const std::bad_any_cast &e)
	{
		logtd("An incorrect type of packet was input from the stream. (%s)", e.what());

		return false;
	}

	if (_writer != nullptr)
	{
		bool ret = _writer->PutData(
			session_packet->GetTrackId(),
			session_packet->GetPts(),
			session_packet->GetDts(),
			session_packet->GetFlag(),
			session_packet->GetBitstreamFormat(),
			session_packet->GetData());

		if (ret == false)
		{
			SetState(SessionState::Error);
			GetRecord()->SetState(info::Record::RecordState::Error);

			_writer->Stop();
			_writer = nullptr;

			return false;
		}

		GetRecord()->UpdateRecordTime();
		GetRecord()->IncreaseRecordBytes(session_packet->GetData()->GetLength());
	}

	bool need_file_split = false;

	// When setting interval parameter, perform segmentation recording.
	if ((uint64_t)GetRecord()->GetInterval() > 0 && (GetRecord()->GetRecordTime() > (uint64_t)GetRecord()->GetInterval()))
	{
		need_file_split = true;
	}
	// When setting schedule parameter, perform segmentation recording.
	else if (GetRecord()->GetSchedule().IsEmpty() != true)
	{
		if (GetRecord()->IsNextScheduleTimeEmpty() == true)
		{
			if (GetRecord()->UpdateNextScheduleTime() == false)
			{
				logte("Failed to update next schedule time. reqeuset to stop recording.");
			}
		}
		else if (GetRecord()->GetNextScheduleTime() <= std::chrono::system_clock::now())
		{
			need_file_split = true;

			if (GetRecord()->UpdateNextScheduleTime() == false)
			{
				logte("Failed to update next schedule time. reqeuset to stop recording.");
			}
		}
	}

	if (need_file_split)
	{
		Split();
	}

	return true;
}

void FileSession::OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
								   const std::shared_ptr<const ov::Data> &data)
{
	// Not used
}

void FileSession::SetRecord(std::shared_ptr<info::Record> &record)
{
	_record = record;
}

std::shared_ptr<info::Record> &FileSession::GetRecord()
{
	return _record;
}

ov::String FileSession::GetRootPath()
{
	auto app_config = std::static_pointer_cast<info::Application>(GetApplication())->GetConfig();
	auto file_config = app_config.GetPublishers().GetFilePublisher();

	return file_config.GetRootPath();
}

ov::String FileSession::GetOutputTempFilePath(std::shared_ptr<info::Record> &record)
{
	ov::String tmp_directory = ov::PathManager::ExtractPath(record->GetOutputFilePath());
	ov::String tmp_filename = ov::String::FormatString("tmp_%s", ov::Random::GenerateString(32).CStr());

	return ov::PathManager::Combine(tmp_directory, tmp_filename);
}

ov::String FileSession::GetOutputFilePath()
{
	auto app_config = std::static_pointer_cast<info::Application>(GetApplication())->GetConfig();
	auto file_config = app_config.GetPublishers().GetFilePublisher();

	ov::String template_path = "";
	if (GetRecord()->IsFilePathSetByUser() == true)
	{
		template_path = GetRecord()->GetFilePath();
	}
	else
	{
		template_path = file_config.GetFilePath();
	}

	// If FilePath config is not set, save it as the default path.
	if (template_path.GetLength() == 0 || template_path.IsEmpty() == true)
	{
		template_path.Format("%s${TransactionId}_${VirtualHost}_${Application}_${Stream}_${StartTime:YYYYMMDDhhmmss}_${EndTime:YYYYMMDDhhmmss}.ts", ov::PathManager::GetAppPath("records").CStr());
	}

	auto result = ConvertMacro(template_path);

	return result;
}

ov::String FileSession::GetOutputFileInfoPath()
{
	auto app_config = std::static_pointer_cast<info::Application>(GetApplication())->GetConfig();
	auto file_config = app_config.GetPublishers().GetFilePublisher();

	ov::String template_path = "";

	if (GetRecord()->IsInfoPathSetByUser() == true)
	{
		template_path = GetRecord()->GetInfoPath();
	}
	else
	{
		template_path = file_config.GetInfoPath();
	}

	// If FileInfoPath config is not set, save it as the default path.
	if (template_path.GetLength() == 0 || template_path.IsEmpty() == true)
	{
		template_path.Format("%s${TransactionId}_${VirtualHost}_${Application}_${Stream}.xml", ov::PathManager::GetAppPath("records").CStr());
	}

	auto result = ConvertMacro(template_path);

	return result;
}

bool FileSession::MakeDirectoryRecursive(std::string s)
{
	size_t pos = 0;
	std::string dir;

	if (access(s.c_str(), R_OK | W_OK) == 0)
	{
		return true;
	}

	if (s[s.size() - 1] != '/')
	{
		// force trailing / so we can handle everything in loop
		s += '/';
	}

	while ((pos = s.find_first_of('/', pos)) != std::string::npos)
	{
		dir = s.substr(0, pos++);

		logtd("* %s", dir.c_str());

		if (dir.size() == 0)
			continue;  // if leading / first time is 0 length

		// If you want to change the directory permission, 
		// change the mask parameter of the MakeDirectory function.
		if (ov::PathManager::MakeDirectory(dir.c_str()) == false)
		{
			return false;
		}
	}

	if (access(s.c_str(), R_OK | W_OK) == 0)
	{
		return true;
	}

	return false;
}

ov::String FileSession::ConvertMacro(ov::String src)
{
	auto app = std::static_pointer_cast<info::Application>(GetApplication());
	auto stream = std::static_pointer_cast<info::Stream>(GetStream());
	auto pub_config = app->GetConfig().GetPublishers();
	auto host_config = app->GetHostInfo();

	std::string raw_string = src.CStr();
	ov::String replaced_string = ov::String(raw_string.c_str());

	// =========================================
	// Definition of Macro
	// =========================================
	// ${StartTime:YYYYMMDDhhmmss}
	// ${EndTime:YYYYMMDDhhmmss}
	// 	 YYYY - year
	// 	 MM - month (00~12)
	// 	 DD - day (00~31)
	// 	 hh : hour (0~23)
	// 	 mm : minute (00~59)
	// 	 ss : second (00~59)
	// ${VirtualHost} :  Virtual Host Name
	// ${Application} : Application Name
	// ${Stream} : Stream name
	// ${Sequence} : Sequence number
	// ${Id} : Identification Code

	std::regex reg_exp("\\$\\{([a-zA-Z0-9:]+)\\}");
	const std::sregex_iterator it_end;
	for (std::sregex_iterator it(raw_string.begin(), raw_string.end(), reg_exp); it != it_end; ++it)
	{
		std::smatch matches = *it;
		std::string tmp;

		tmp = matches[0];
		ov::String full_match = ov::String(tmp.c_str());

		tmp = matches[1];
		ov::String group = ov::String(tmp.c_str());

		// logtd("Full Match(%s) => Group(%s)", full_match.CStr(), group.CStr());

		if (group.IndexOf("VirtualHost") != -1L)
		{
			replaced_string = replaced_string.Replace(full_match, host_config.GetName());
		}
		if (group.IndexOf("Application") != -1L)
		{
			// Delete Prefix virtualhost name. ex) #[VirtualHost]#Application
			ov::String prefix = ov::String::FormatString("#%s#", host_config.GetName().CStr());
			auto app_name = app->GetName();
			auto application_name = app_name.ToString().Replace(prefix, "");

			replaced_string = replaced_string.Replace(full_match, application_name);
		}
		if (group.IndexOf("Stream") != -1L)
		{
			replaced_string = replaced_string.Replace(full_match, stream->GetName());
		}
		if (group.IndexOf("Sequence") != -1L)
		{
			ov::String buff = ov::String::FormatString("%d", GetRecord()->GetSequence());

			replaced_string = replaced_string.Replace(full_match, buff);
		}
		if (group.IndexOf("Id") == 0)
		{
			ov::String buff = ov::String::FormatString("%s", GetRecord()->GetId().CStr());

			replaced_string = replaced_string.Replace(full_match, buff);
		}
		if (group.IndexOf("TransactionId") == 0)
		{
			ov::String buff = ov::String::FormatString("%s", GetRecord()->GetTransactionId().CStr());

			replaced_string = replaced_string.Replace(full_match, buff);
		}
		if (group.IndexOf("StartTime") != -1L)
		{
			time_t now = std::chrono::system_clock::to_time_t(GetRecord()->GetRecordStartTime());
			struct tm timeinfo;
			if (localtime_r(&now, &timeinfo) == nullptr)
			{
				logtw("Could not get localtime");
				continue;
			}

			char buff[80];
			ov::String YYYY, MM, DD, hh, mm, ss;

			strftime(buff, sizeof(buff), "%Y", &timeinfo);
			YYYY = buff;
			strftime(buff, sizeof(buff), "%m", &timeinfo);
			MM = buff;
			strftime(buff, sizeof(buff), "%d", &timeinfo);
			DD = buff;
			strftime(buff, sizeof(buff), "%H", &timeinfo);
			hh = buff;
			strftime(buff, sizeof(buff), "%M", &timeinfo);
			mm = buff;
			strftime(buff, sizeof(buff), "%S", &timeinfo);
			ss = buff;

			ov::String str_time = group;
			str_time = str_time.Replace("StartTime:", "");
			str_time = str_time.Replace("YYYY", YYYY);
			str_time = str_time.Replace("MM", MM);
			str_time = str_time.Replace("DD", DD);
			str_time = str_time.Replace("hh", hh);
			str_time = str_time.Replace("mm", mm);
			str_time = str_time.Replace("ss", ss);

			replaced_string = replaced_string.Replace(full_match, str_time);
		}
		if (group.IndexOf("EndTime") != -1L)
		{
			time_t now = std::chrono::system_clock::to_time_t(GetRecord()->GetRecordStopTime());
			struct tm timeinfo;
			if (localtime_r(&now, &timeinfo) == nullptr)
			{
				logtw("Could not get localtime");
				continue;
			}

			char buff[80];
			ov::String YYYY, MM, DD, hh, mm, ss;

			strftime(buff, sizeof(buff), "%Y", &timeinfo);
			YYYY = buff;
			strftime(buff, sizeof(buff), "%m", &timeinfo);
			MM = buff;
			strftime(buff, sizeof(buff), "%d", &timeinfo);
			DD = buff;
			strftime(buff, sizeof(buff), "%H", &timeinfo);
			hh = buff;
			strftime(buff, sizeof(buff), "%M", &timeinfo);
			mm = buff;
			strftime(buff, sizeof(buff), "%S", &timeinfo);
			ss = buff;

			ov::String str_time = group;
			str_time = str_time.Replace("EndTime:", "");
			str_time = str_time.Replace("YYYY", YYYY);
			str_time = str_time.Replace("MM", MM);
			str_time = str_time.Replace("DD", DD);
			str_time = str_time.Replace("hh", hh);
			str_time = str_time.Replace("mm", mm);
			str_time = str_time.Replace("ss", ss);

			replaced_string = replaced_string.Replace(full_match, str_time);
		}
	}

	// logtd("Regular Expreesion Result : %s", replaced_string.CStr());

	return replaced_string;
}
