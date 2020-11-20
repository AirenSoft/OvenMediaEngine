#include <base/info/stream.h>
#include <base/info/application.h>
#include <base/info/host.h>
#include <base/publisher/stream.h>
#include <base/publisher/application.h>
#include <config/config.h>

#include "file_session.h"
#include "file_private.h"
#include "file_export.h"

#include <regex>

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
	Stop();
	logtd("FileSession(%d) has been terminated finally", GetId());
}


bool FileSession::Start()
{
	logtd("FileSession(%d) has started.", GetId());

	GetRecord()->UpdateRecordStartTime();
	GetRecord()->SetFilePath(GetOutputFilePath());
	GetRecord()->SetTmpPath(GetOutputTempFilePath());
	GetRecord()->SetFileInfoPath(GetOutputFileInfoPath());
	GetRecord()->SetState(info::Record::RecordState::Recording);

	// Create directory for temporary file
	ov::String tmp_directory = ov::PathManager::ExtractPath(GetOutputTempFilePath());
	if (MakeDirectoryRecursive(tmp_directory.CStr()) == false)
	{
		logte("Could not create directory. path(%s)", tmp_directory.CStr());

		SetState(SessionState::Error);	
		GetRecord()->SetState(info::Record::RecordState::Error);		

		return false;
	}

	_writer = FileWriter::Create();
	if(_writer == nullptr)
	{
		SetState(SessionState::Error);	
		GetRecord()->SetState(info::Record::RecordState::Error);		

		return false;
	}

	if(_writer->SetPath(GetRecord()->GetTmpPath(), "mpegts") == false)
	{
		SetState(SessionState::Error);
		GetRecord()->SetState(info::Record::RecordState::Error);		

		_writer = nullptr;

		return false;
	}

	for(auto &track_item : GetStream()->GetTracks())
	{
		auto &track = track_item.second;

		// If the selected track list exists. if the current trackid does not exist on the list, ignore it.
		// If no track list is selected, save all tracks.
		if( (selected_tracks.empty() != true) && 
			(std::find(selected_tracks.begin(), selected_tracks.end(), track->GetId()) == selected_tracks.end()) )
		{
			continue;
		}

		auto track_info = FileTrackInfo::Create();

		track_info->SetCodecId( track->GetCodecId() );
		track_info->SetBitrate( track->GetBitrate() );
		track_info->SetTimeBase( track->GetTimeBase() );
		track_info->SetWidth( track->GetWidth() );
		track_info->SetHeight( track->GetHeight() );
		track_info->SetSample( track->GetSample() );
		track_info->SetChannel( track->GetChannel() );
		track_info->SetExtradata( track->GetCodecExtradata() );

		bool ret = _writer->AddTrack(track->GetMediaType(), track->GetId(), track_info);
		if(ret == false)
		{
			logtw("Failed to add media track");
		}
	}

	if(_writer->Start() == false)
	{
		_writer = nullptr;
		SetState(SessionState::Error);
		GetRecord()->SetState(info::Record::RecordState::Error);		

		return false;
	}

	return Session::Start();
}

bool FileSession::Stop()
{
	if(_writer != nullptr)
	{
		SetState(SessionState::Stopping);			
		GetRecord()->SetState(info::Record::RecordState::Stopping);

		GetRecord()->UpdateRecordStopTime();
		GetRecord()->SetFilePath(GetOutputFilePath());
		GetRecord()->SetFileInfoPath(GetOutputFileInfoPath());

		_writer->Stop();

		ov::String tmp_output_path = _writer->GetPath();

		// Create directory
		ov::String output_path = GetRecord()->GetFilePath();
		ov::String output_direcotry = ov::PathManager::ExtractPath(output_path);

		if (MakeDirectoryRecursive(output_direcotry.CStr()) == false)
		{
			logte("Could not create directory. path(%s)", output_direcotry.CStr());

			SetState(SessionState::Error);	
			GetRecord()->SetState(info::Record::RecordState::Error);		

			return false;
		}

		ov::String info_path = GetRecord()->GetFileInfoPath();
		ov::String info_directory = ov::PathManager::ExtractPath(info_path);

		if (MakeDirectoryRecursive(info_directory.CStr()) == false)
		{
			logte("Could not create directory. path(%s)", info_directory.CStr());

			SetState(SessionState::Error);			
			GetRecord()->SetState(info::Record::RecordState::Error);		

			return false;
		}

		// Moves temporary files to a user-defined path.
		if (rename( tmp_output_path.CStr() , output_path.CStr() ) != 0)
		{
			logte("Failed to move file. %s -> %s", tmp_output_path.CStr() , output_path.CStr());

			SetState(SessionState::Error);
			GetRecord()->SetState(info::Record::RecordState::Error);		

			return false;
		}

		if( FileExport::GetInstance()->ExportRecordToXml(GetRecord()->GetFileInfoPath(), GetRecord()) == false )
		{
			logte("Failed to export xml file. path(%s)", GetRecord()->GetFileInfoPath().CStr());
		}


		GetRecord()->SetState(info::Record::RecordState::Stopped);
		GetRecord()->IncreaseSequence();

		_writer = nullptr;

		logtd("FileSession(%d) has stoped", GetId());
	}

	return Session::Stop();
}

bool FileSession::SendOutgoingData(const std::any &packet)
{
	std::shared_ptr<MediaPacket> session_packet;

	try 
	{
        session_packet = std::any_cast<std::shared_ptr<MediaPacket>>(packet);
		if(session_packet == nullptr)
		{
			return false;
		}
    }
    catch(const std::bad_any_cast& e) 
	{
        logtd("An incorrect type of packet was input from the stream. (%s)", e.what());

		return false;
    }

	if(_writer != nullptr)
    {
	  	bool ret = _writer->PutData(
			session_packet->GetTrackId(), 
			session_packet->GetPts(),
			session_packet->GetDts(), 
			session_packet->GetFlag(), 
			session_packet->GetData());

		if(ret == false)
		{
			logte("Failed to add packet");
			SetState(SessionState::Error);
			GetRecord()->SetState(info::Record::RecordState::Error);

			_writer->Stop();
			_writer = nullptr;

			return false;
		} 

		GetRecord()->UpdateRecordTime();
		GetRecord()->IncreaseRecordBytes(session_packet->GetData()->GetLength());
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

std::shared_ptr<info::Record>& FileSession::GetRecord()
{
	return _record;
}

ov::String FileSession::GetOutputTempFilePath()
{
	auto app_config = std::static_pointer_cast<info::Application>(GetApplication())->GetConfig();
	auto file_config = app_config.GetPublishers().GetFilePublisher();

	auto path = ConvertMacro(file_config.GetFilePath()) + ".tmp";

	return path;
}

ov::String FileSession::GetOutputFilePath()
{
	auto app_config = std::static_pointer_cast<info::Application>(GetApplication())->GetConfig();
	auto file_config = app_config.GetPublishers().GetFilePublisher();

	auto path = ConvertMacro(file_config.GetFilePath());
	
	return path;
}

ov::String FileSession::GetOutputFileInfoPath()
{
	auto app_config = std::static_pointer_cast<info::Application>(GetApplication())->GetConfig();
	auto file_config = app_config.GetPublishers().GetFilePublisher();

	auto path = ConvertMacro(file_config.GetFileInfoPath());

	return path;
}

bool FileSession::MakeDirectoryRecursive(std::string s, mode_t mode)
{
	size_t pos = 0;
	std::string dir;
	int32_t mdret;

	if(access(s.c_str(), R_OK | W_OK ) == 0)
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
			continue; // if leading / first time is 0 length

		if ((mdret = ::mkdir(dir.c_str(), mode)) && errno != EEXIST)
		{
			logtd("* ret : %d", mdret);
			return false;
		}
	}

	if(access(s.c_str(), R_OK | W_OK ) == 0)
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
	// Deifinitino of Macro
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
	// ${Id} : Idenficiation Code

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

		if(group.IndexOf("VirtualHost") != -1L)
		{
			replaced_string = replaced_string.Replace(full_match, host_config.GetName());
		}
		if(group.IndexOf("Application") != -1L)
		{
			// Delete Prefix virtualhost name. ex) #[VirtualHost]#Application
			ov::String prefix = ov::String::FormatString("#%s#", host_config.GetName().CStr());
			auto app_name =  app->GetName();
			auto application_name = app_name.ToString().Replace(prefix, "");

			replaced_string = replaced_string.Replace(full_match, application_name);
		}
		if(group.IndexOf("Stream") != -1L)
		{
			replaced_string = replaced_string.Replace(full_match, stream->GetName());
		}
		if(group.IndexOf("Sequence") != -1L)
		{
			ov::String buff = ov::String::FormatString("%d", GetRecord()->GetSequence());

			replaced_string = replaced_string.Replace(full_match, buff);
		}		
		if(group.IndexOf("Id") != -1L)
		{
			ov::String buff = ov::String::FormatString("%s", GetRecord()->GetId().CStr());

			replaced_string = replaced_string.Replace(full_match, buff);
		}				
		if(group.IndexOf("StartTime") != -1L)
		{
			time_t now = std::chrono::system_clock::to_time_t(GetRecord()->GetRecordStartTime());
			struct tm timeinfo;
			if(localtime_r(&now, &timeinfo) == nullptr)
			{
				logtw("Could not get localtime");
				continue;
			}

			char buff[80];
			ov::String YYYY, MM, DD, hh, mm, ss;

			strftime(buff, sizeof(buff), "%Y", &timeinfo); YYYY = buff;
			strftime(buff, sizeof(buff), "%m", &timeinfo); MM = buff;
			strftime(buff, sizeof(buff), "%d", &timeinfo); DD = buff;
			strftime(buff, sizeof(buff), "%H", &timeinfo); hh = buff;
			strftime(buff, sizeof(buff), "%M", &timeinfo); mm = buff;
			strftime(buff, sizeof(buff), "%S", &timeinfo); ss = buff;			

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
		if(group.IndexOf("EndTime") != -1L)
		{
			time_t now = std::chrono::system_clock::to_time_t(GetRecord()->GetRecordStopTime());
			struct tm timeinfo;
			if(localtime_r(&now, &timeinfo) == nullptr)
			{
				logtw("Could not get localtime");
				continue;
			}

			char buff[80];
			ov::String YYYY, MM, DD, hh, mm, ss;

			strftime(buff, sizeof(buff), "%Y", &timeinfo); YYYY = buff;
			strftime(buff, sizeof(buff), "%m", &timeinfo); MM = buff;
			strftime(buff, sizeof(buff), "%d", &timeinfo); DD = buff;
			strftime(buff, sizeof(buff), "%H", &timeinfo); hh = buff;
			strftime(buff, sizeof(buff), "%M", &timeinfo); mm = buff;
			strftime(buff, sizeof(buff), "%S", &timeinfo); ss = buff;			

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


