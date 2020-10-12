#include "file_private.h"
#include "file_stream.h"
#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include <regex>

std::shared_ptr<FileStream> FileStream::Create(const std::shared_ptr<pub::Application> application,
											 const info::Stream &info)
{
	auto stream = std::make_shared<FileStream>(application, info);
	if(!stream->Start())
	{
		return nullptr;
	}
	return stream;
}

FileStream::FileStream(const std::shared_ptr<pub::Application> application,
					 const info::Stream &info)
		: Stream(application, info),
		_writer(nullptr)
{
}

FileStream::~FileStream()
{
	logtd("FileStream(%s/%s) has been terminated finally", 
		GetApplicationName() , GetName().CStr());
}

bool FileStream::Start()
{
	logtd("FileStream(%ld) has been started", GetId());

	//-----------------
	// for test
	// std::vector<int32_t> selected_tracks;
	// RecordStart(selected_tracks);
	//-----------------

	return Stream::Start();
}

bool FileStream::Stop()
{
	logtd("FileStream(%u) has been stopped", GetId());

	//-----------------
	// for test
	// RecordStop();
	//-----------------

	return Stream::Stop();
}


void FileStream::RecordStart(std::vector<int32_t> selected_tracks)
{
	_writer = FileWriter::Create();

	ov::String tmp_output_fullpath = GetOutputTempFilePath();
	ov::String tmp_output_directory = ov::PathManager::ExtractPath(tmp_output_fullpath).CStr();

	logtd("Temp output path : %s", tmp_output_fullpath.CStr());
	logtd("Temp output directory : %s", tmp_output_directory.CStr());

	// Create temporary directory
	if(MakeDirectoryRecursive(tmp_output_directory.CStr()) == false)
	{
		logte("Could not create directory. path(%s)", tmp_output_directory.CStr());
		
		return;
	}

	// Record it on a temporary path.
	if(_writer->SetPath(tmp_output_fullpath, "mpegts") == false)
	{
		_writer = nullptr;

		return;
	}

	for(auto &track_item : _tracks)
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
			logte("Failed to add new track");
		}
	}

	if(_writer->Start() == false)
	{
		_writer = nullptr;
	}
}

void FileStream::RecordStop()
{
	if(_writer == nullptr)
		return;

	// End recording.
	_writer->Stop();

	ov::String tmp_output_path = _writer->GetPath();

	// Create directory
	ov::String output_path = GetOutputFilePath();
	ov::String output_direcotry = ov::PathManager::ExtractPath(output_path);

	if(MakeDirectoryRecursive(output_direcotry.CStr()) == false)
	{
		logte("Could not create directory. path(%s)", output_direcotry.CStr());	
		return;
	}

	ov::String info_path = GetOutputFileInfoPath();
	ov::String info_directory = ov::PathManager::ExtractPath(info_path);
	
	if(MakeDirectoryRecursive(info_directory.CStr()) == false)
	{
		logte("Could not create directory. path(%s)", info_directory.CStr());	
		return;
	}

	// Moves temporary files to a user-defined path.
	if(rename( tmp_output_path.CStr() , output_path.CStr() ) != 0)
	{
		logte("Failed to move file. %s -> %s", tmp_output_path.CStr() , output_path.CStr());
		return;
	}

	// Add information from the recored file.
	// TODO:

	logtd("File Recording Successful.");
}

void FileStream::RecordStat()
{
	// TODO: Get Statistics
	
}

void FileStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	//-----------------
	// for test
	// if(_writer == nullptr)
	// {
	// 	std::vector<int32_t> selected_tracks;
	// 	RecordStart(selected_tracks);
	// }
	//-----------------
	
	if(_writer == nullptr)
		return;

	bool ret = _writer->PutData(
		media_packet->GetTrackId(), 
		media_packet->GetPts(),
		media_packet->GetDts(), 
		media_packet->GetFlag(), 
		media_packet->GetData());

	if(ret == false)
	{
		logte("Failed to add packet");
	}	
}

void FileStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if(_writer == nullptr)
		return;

	bool ret = _writer->PutData(
		media_packet->GetTrackId(), 
		media_packet->GetPts(),
		media_packet->GetDts(), 
		media_packet->GetFlag(), 
		media_packet->GetData());

	if(ret == false)
	{
		logte("Failed to add packet");
	}	
}

ov::String FileStream::GetOutputTempFilePath()
{
	auto file_config = GetApplicationInfo().GetConfig().GetPublishers().GetFilePublisher();


	ov::String tmp_path = ConvertMacro(file_config.GetFilePath()) + ".tmp";

	logtd("TempFile Path : %s", tmp_path.CStr());

	return tmp_path;
}

ov::String FileStream::GetOutputFilePath()
{
	auto file_config = GetApplicationInfo().GetConfig().GetPublishers().GetFilePublisher();

	logtd("File Path : %s", file_config.GetFilePath().CStr());

	return ConvertMacro(file_config.GetFilePath());
}

ov::String FileStream::GetOutputFileInfoPath()
{
	auto file_config = GetApplicationInfo().GetConfig().GetPublishers().GetFilePublisher();

	logtd("FileInfo Path : %s", file_config.GetFileInfoPath().CStr());

	return ConvertMacro(file_config.GetFileInfoPath());
}

bool FileStream::MakeDirectoryRecursive(std::string s, mode_t mode)
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


ov::String FileStream::ConvertMacro(ov::String src)
{
	auto app_config = GetApplicationInfo().GetConfig();
	auto publishers_config = app_config.GetPublishers();
	auto host_info = GetApplicationInfo().GetHostInfo();

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

		logtd("Full Match(%s) => Group(%s)", full_match.CStr(), group.CStr());

		if(group.IndexOf("VirtualHost") != -1L)
		{
			replaced_string = replaced_string.Replace(full_match, host_info.GetName());
		}
		if(group.IndexOf("Application") != -1L)
		{
			// Delete Prefix virtualhost name. ex) #[VirtualHost]#Application
			ov::String prefix = ov::String::FormatString("#%s#", host_info.GetName().CStr());
			auto app_name =  GetApplicationInfo().GetName();
			auto application_name = app_name.ToString().Replace(prefix, "");

			replaced_string = replaced_string.Replace(full_match, application_name);
		}
		if(group.IndexOf("Stream") != -1L)
		{
			replaced_string = replaced_string.Replace(full_match, GetName());
		}
		if(group.IndexOf("Sequence") != -1L)
		{
			replaced_string = replaced_string.Replace(full_match, "0");
		}		
		if(group.IndexOf("StartTime") != -1L)
		{
			time_t now = time(NULL);
			char buff[80];
			ov::String YYYY, MM, DD, hh, mm, ss;

			strftime(buff, sizeof(buff), "%Y", localtime(&now)); YYYY = buff;
			strftime(buff, sizeof(buff), "%m", localtime(&now)); MM = buff;
			strftime(buff, sizeof(buff), "%d", localtime(&now)); DD = buff;
			strftime(buff, sizeof(buff), "%H", localtime(&now)); hh = buff;
			strftime(buff, sizeof(buff), "%M", localtime(&now)); mm = buff;
			strftime(buff, sizeof(buff), "%S", localtime(&now)); ss = buff;			

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
			time_t now = time(NULL);
			char buff[80];
			ov::String YYYY, MM, DD, hh, mm, ss;

			strftime(buff, sizeof(buff), "%Y", localtime(&now)); YYYY = buff;
			strftime(buff, sizeof(buff), "%m", localtime(&now)); MM = buff;
			strftime(buff, sizeof(buff), "%d", localtime(&now)); DD = buff;
			strftime(buff, sizeof(buff), "%H", localtime(&now)); hh = buff;
			strftime(buff, sizeof(buff), "%M", localtime(&now)); mm = buff;
			strftime(buff, sizeof(buff), "%S", localtime(&now)); ss = buff;			

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

	logtd("Regular Expreesion Result : %s", replaced_string.CStr());

	return replaced_string;
}


