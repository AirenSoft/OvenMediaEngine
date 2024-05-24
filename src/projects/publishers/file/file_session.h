#pragma once

#include <base/info/media_track.h>
#include <base/publisher/session.h>
#include <modules/ffmpeg/ffmpeg_writer.h>

#include "base/info/record.h"

namespace pub
{
	class FileSession : public pub::Session
	{
	public:
		static std::shared_ptr<FileSession> Create(const std::shared_ptr<pub::Application> &application,
												   const std::shared_ptr<pub::Stream> &stream,
												   uint32_t ovt_session_id);

		FileSession(const info::Session &session_info,
					const std::shared_ptr<pub::Application> &application,
					const std::shared_ptr<pub::Stream> &stream);
		~FileSession() override;

		bool Start() override;
		bool Stop() override;

		bool Split();
		bool StartRecord();
		bool StopRecord();

		void SendOutgoingData(const std::any &packet) override;

		void SetRecord(std::shared_ptr<info::Record> &record);
		std::shared_ptr<info::Record> &GetRecord();

	private:
		ov::String GetRootPath();
		ov::String GetOutputTempFilePath(std::shared_ptr<info::Record> &record);
		ov::String GetOutputFilePath();
		ov::String GetOutputFileInfoPath();
		ov::String ConvertMacro(ov::String src);

		bool IsSelectedTrack(const std::shared_ptr<MediaTrack> &track);
		void SelectDefaultTrack(const std::shared_ptr<MediaTrack> &track);

		bool MakeDirectoryRecursive(std::string s);

	private:
		std::shared_ptr<ffmpeg::Writer> _writer;

		std::shared_ptr<info::Record> _record;

		std::shared_mutex _lock;

		std::map<cmn::MediaType, MediaTrackId> _default_track_by_type;
		MediaTrackId _default_track;
		bool _found_first_keyframe = false;
	};
}  // namespace pub