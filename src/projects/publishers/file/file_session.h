#pragma once

#include <base/info/media_track.h>
#include <base/publisher/session.h>
#include <modules/file/file_writer.h>

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

	bool SendOutgoingData(const std::any &packet) override;
	void OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
						const std::shared_ptr<const ov::Data> &data) override;

	void SetSelectTracks(std::vector<int32_t> &tracks);

private:
	ov::String GetOutputTempFilePath();
	ov::String GetOutputFilePath();
	ov::String GetOutputFileInfoPath();
	ov::String ConvertMacro(ov::String src);
	bool MakeDirectoryRecursive(std::string s,mode_t mode = S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

private:
	bool _sent_ready;

	std::vector<int32_t> selected_tracks;

	std::shared_ptr<FileWriter> _writer;

	// statistics
	int64_t _stats_sent_bytes;
	int64_t _stats_sent_packets;
	int32_t _stats_recoonect;
	
};
