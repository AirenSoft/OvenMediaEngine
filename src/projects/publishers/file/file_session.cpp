#include "file_session.h"

#include <base/info/application.h>
#include <base/info/host.h>
#include <base/info/stream.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/publisher/application.h>
#include <base/publisher/stream.h>
#include <config/config.h>
#include <modules/ffmpeg/ffmpeg_conv.h>
#include <modules/ffmpeg/ffmpeg_writer.h>

#include <regex>

#include "file_export.h"
#include "file_private.h"
#include "file_macro.h"

namespace pub
{
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
		MonitorInstance->OnSessionConnected(*stream, PublisherType::File);
	}

	FileSession::~FileSession()
	{
		logtd("FileSession(%d) has been terminated finally", GetId());
		MonitorInstance->OnSessionDisconnected(*GetStream(), PublisherType::File);
	}

	bool FileSession::Start()
	{
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
		auto record = GetRecord();
		if (record == nullptr)
		{
			logte("Record information is not set. id(%d)", GetId());
			return false;
		}

		record->UpdateRecordStartTime();
		record->SetOutputFilePath(GetOutputFilePath());
		record->SetOutputInfoPath(GetOutputFileInfoPath());
		record->SetTmpPath(GetOutputTempFilePath(record));
		record->SetState(info::Record::RecordState::Recording);

		// Get extension and container format
		ov::String output_extension = ov::PathManager::ExtractExtension(record->GetOutputFilePath());
		ov::String output_format = ffmpeg::Conv::GetFormatByExtension(output_extension, "mpegts");

		// Create directory for temporary file
		ov::String tmp_directory = ov::PathManager::ExtractPath(record->GetTmpPath());
		ov::String tmp_real_directory = ov::PathManager::Combine(GetRootPath(), tmp_directory);

		if (ov::PathManager::MakeDirectoryRecursive(tmp_real_directory.CStr()) == false)
		{
			logte("Could not create directory. path(%s)", tmp_real_directory.CStr());

			SetState(SessionState::Error);
			record->SetState(info::Record::RecordState::Error);

			return false;
		}

		auto writer = CreateWriter();
		if (writer == nullptr)
		{
			SetState(SessionState::Error);
			record->SetState(info::Record::RecordState::Error);

			return false;
		}

		if (writer->SetUrl(ov::PathManager::Combine(GetRootPath(), record->GetTmpPath()), output_format) == false)
		{
			SetState(SessionState::Error);
			record->SetState(info::Record::RecordState::Error);

			return false;
		}

		// The mode to specify the initial value of the timestamp stored in the file to zero,
		// or keep it at the same value as the source timestamp
		if (record->GetSegmentationRule() == "continuity")
		{
			writer->SetTimestampMode(ffmpeg::Writer::TIMESTAMP_PASSTHROUGH_MODE);
		}
		else if (record->GetSegmentationRule() == "discontinuity")
		{
			writer->SetTimestampMode(ffmpeg::Writer::TIMESTAMP_STARTZERO_MODE);
		}

		for (auto &[track_id, track] : GetStream()->GetTracks())
		{
			if(IsSelectedTrack(track) == false)
			{
				continue;
			}

			if (ffmpeg::Conv::IsSupportCodec(output_format, track->GetCodecId()) == false)
			{
				logtd("%s format does not support the codec(%s)", output_format.CStr(), cmn::GetCodecIdToString(track->GetCodecId()).CStr());
				continue;
			}

			// Choose default track of recording stream
			SelectDefaultTrack(track);

			bool ret = writer->AddTrack(track);
			if (ret == false)
			{
				logtw("Failed to add new track");
			}
		}

		logtd("Create temporary file(%s) and default track id(%d)", writer->GetUrl().CStr(), _default_track);

		if (writer->Start() == false)
		{
			SetState(SessionState::Error);
			record->SetState(info::Record::RecordState::Error);

			return false;
		}

		logtd("Recording Started. %s", record->GetInfoString().CStr());

		return true;
	}

	// Select the first video track as the default track.
	// If there is no video track, the first track of the audio is selected as the default track.
	void FileSession::SelectDefaultTrack(const std::shared_ptr<MediaTrack> &track)
	{
		if (_default_track_by_type.find(track->GetMediaType()) == _default_track_by_type.end())
		{
			_default_track_by_type[track->GetMediaType()] = track->GetId();

			if (_default_track_by_type.find(cmn::MediaType::Video) != _default_track_by_type.end())
			{
				_default_track = _default_track_by_type[cmn::MediaType::Video];
			}
			else if (_default_track_by_type.find(cmn::MediaType::Audio) != _default_track_by_type.end())
			{
				_default_track = _default_track_by_type[cmn::MediaType::Audio];
			}
		}
	}

	// Check if the track is selected for recording.
	bool FileSession::IsSelectedTrack(const std::shared_ptr<MediaTrack> &track)
	{
		auto record = GetRecord();
		if (record == nullptr)
		{
			return false;
		}

		auto selected_track_ids = record->GetTrackIds();
		auto selected_track_names = record->GetVariantNames();
		if (selected_track_ids.size() > 0 || selected_track_names.size() > 0)
		{
			if ((find(selected_track_ids.begin(), selected_track_ids.end(), track->GetId()) == selected_track_ids.end()) &&
				(find(selected_track_names.begin(), selected_track_names.end(), track->GetVariantName()) == selected_track_names.end()))
			{
				return false;
			}
		}

		return true;
	}

	bool FileSession::StopRecord()
	{
		auto writer = GetWriter();
		if (writer != nullptr)
		{
			writer->Stop();

			SetState(SessionState::Stopping);

			auto record = GetRecord();
			if (record)
			{
				record->SetState(info::Record::RecordState::Stopping);
				record->UpdateRecordStopTime();
				record->SetOutputFilePath(GetOutputFilePath());
				record->SetOutputInfoPath(GetOutputFileInfoPath());

				// Create directory for recorded file
				ov::String output_path = ov::PathManager::Combine(GetRootPath(), record->GetOutputFilePath());
				ov::String output_directory = ov::PathManager::ExtractPath(output_path);

				if (ov::PathManager::MakeDirectoryRecursive(output_directory.CStr()) == false)
				{
					logte("Could not create directory. path: %s", output_directory.CStr());

					SetState(SessionState::Error);
					record->SetState(info::Record::RecordState::Error);

					return false;
				}

				// Create directory for information file
				ov::String info_path = ov::PathManager::Combine(GetRootPath(), record->GetOutputInfoPath());
				ov::String info_directory = ov::PathManager::ExtractPath(info_path);

				if (ov::PathManager::MakeDirectoryRecursive(info_directory.CStr()) == false)
				{
					logte("Could not create directory. path: %s", info_directory.CStr());

					SetState(SessionState::Error);
					record->SetState(info::Record::RecordState::Error);

					return false;
				}

				// Moves temporary files to a user-defined path.
				ov::String tmp_output_path = writer->GetUrl();

				if (rename(tmp_output_path.CStr(), output_path.CStr()) != 0)
				{
					logte("Failed to move file. from: %s to: %s", tmp_output_path.CStr(), output_path.CStr());

					SetState(SessionState::Error);
					record->SetState(info::Record::RecordState::Error);

					return false;
				}

				logtd("Replace the temporary file name with the target file name. from: %s, to: %s", tmp_output_path.CStr(), output_path.CStr());

				// Append recorded information to the information file
				if (FileExport::GetInstance()->ExportRecordToXml(info_path, record) == false)
				{
					logte("Failed to export xml file. path: %s", info_path.CStr());
				}

				logtd("Appends the recording result to the information file. path: %s", info_path.CStr());

				record->SetState(info::Record::RecordState::Stopped);
				
				logtd("Recording Completed. %s", record->GetInfoString().CStr());
								
				record->IncreaseSequence();
			}

			DestoryWriter();
		}

		return true;
	}

	void FileSession::SendOutgoingData(const std::any &packet)
	{
		std::shared_ptr<MediaPacket> session_packet;

		try
		{
			session_packet = std::any_cast<std::shared_ptr<MediaPacket>>(packet);
			if (session_packet == nullptr)
			{
				return;
			}
		}
		catch (const std::bad_any_cast &e)
		{
			logtd("An incorrect type of packet was input from the stream. (%s)", e.what());

			return;
		}

		auto record = GetRecord();
		if (!record)
		{
			logte("Record information is not set. id(%d)", GetId());
			return;
		}

		// Drop until the first keyframe of the main track is received.
		if (_found_first_keyframe == false)
		{
			if (_default_track == session_packet->GetTrackId())
			{
				if ((session_packet->GetMediaType() == cmn::MediaType::Audio) ||
					(session_packet->GetMediaType() == cmn::MediaType::Video && session_packet->GetFlag() == MediaPacketFlag::Key))
				{
					_found_first_keyframe = true;
				}
				else
				{
					record->UpdateRecordStartTime();
					return;
				}
			}
		}

		bool need_file_split = false;

		// When setting interval parameter, perform segmentation recording.
		if (((uint64_t)record->GetInterval() > 0) && (record->GetRecordTime() > (uint64_t)record->GetInterval()) &&
			(session_packet->GetTrackId() == _default_track) &&
			((session_packet->GetMediaType() == cmn::MediaType::Audio) ||
			(session_packet->GetMediaType() == cmn::MediaType::Video && session_packet->GetFlag() == MediaPacketFlag::Key)) )
		{
			need_file_split = true;
		}
		// When setting schedule parameter, perform segmentation recording.
		else if (record->GetSchedule().IsEmpty() != true)
		{
			if (record->IsNextScheduleTimeEmpty() == true)
			{
				if (record->UpdateNextScheduleTime() == false)
				{
					logte("Failed to update next schedule time. request to stop recording.");
				}
			}
			else if (record->GetNextScheduleTime() <= std::chrono::system_clock::now())
			{
				need_file_split = true;

				if (record->UpdateNextScheduleTime() == false)
				{
					logte("Failed to update next schedule time. request to stop recording.");
				}
			}
		}

		if (need_file_split)
		{
			Split();
		}

		auto writer = GetWriter();
		if (writer != nullptr)
		{
			uint64_t sent_bytes = 0;

			bool ret = writer->SendPacket(session_packet, &sent_bytes);
			if (ret == false)
			{
				SetState(SessionState::Error);
				record->SetState(info::Record::RecordState::Error);

				DestoryWriter();

				return;
			}

			record->UpdateRecordTime();
			record->IncreaseRecordBytes(sent_bytes);
			
			MonitorInstance->IncreaseBytesOut(*GetStream(), PublisherType::File, sent_bytes);
		}
	}

	void FileSession::SetRecord(std::shared_ptr<info::Record> &record)
	{
		std::lock_guard<std::shared_mutex> mlock(_record_mutex);

		_record = record;
	}

	std::shared_ptr<info::Record> &FileSession::GetRecord()
	{
		std::shared_lock<std::shared_mutex> mlock(_record_mutex);
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
		auto record = GetRecord();
		if(!record)
		{
			logte("Record information is not set. id(%d)", GetId());

			return "";
		}

		ov::String template_path = "";
		if (record->IsFilePathSetByUser() == true)
		{
			template_path = record->GetFilePath();
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

		auto result = FileMacro::ConvertMacro(std::static_pointer_cast<info::Application>(GetApplication()), std::static_pointer_cast<info::Stream>(GetStream()), record, template_path);

		return result;
	}

	ov::String FileSession::GetOutputFileInfoPath()
	{
		auto app_config = std::static_pointer_cast<info::Application>(GetApplication())->GetConfig();
		auto file_config = app_config.GetPublishers().GetFilePublisher();
		auto record = GetRecord();
		if(!record)
		{
			logte("Record information is not set. id(%d)", GetId());
			
			return "";
		}

		ov::String template_path = "";

		if (record->IsInfoPathSetByUser() == true)
		{
			template_path = record->GetInfoPath();
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

		auto result = FileMacro::ConvertMacro(std::static_pointer_cast<info::Application>(GetApplication()), std::static_pointer_cast<info::Stream>(GetStream()), record, template_path);

		return result;
	}

	std::shared_ptr<ffmpeg::Writer> FileSession::CreateWriter()
	{
		std::lock_guard<std::shared_mutex> lock(_writer_mutex);
		if (_writer != nullptr)
		{
			_writer->Stop();
			_writer = nullptr;
		}

		_writer = ffmpeg::Writer::Create();
		if (_writer == nullptr)
		{
			return nullptr;
		}

		return _writer;
	}

	void FileSession::DestoryWriter()
	{
		std::lock_guard<std::shared_mutex> lock(_writer_mutex);
		if (_writer != nullptr)
		{
			_writer->Stop();
			_writer = nullptr;
		}
	}

	std::shared_ptr<ffmpeg::Writer> FileSession::GetWriter()
	{
		std::shared_lock<std::shared_mutex> lock(_writer_mutex);
		return _writer;
	}

}  // namespace pub
