#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>
#include <modules/ovt_packetizer/ovt_packetizer.h>

#include <modules/file/file_writer.h>

#include "monitoring/monitoring.h"

class FileStream : public pub::Stream
{
public:
	static std::shared_ptr<FileStream> Create(const std::shared_ptr<pub::Application> application,
											 const info::Stream &info);

	explicit FileStream(const std::shared_ptr<pub::Application> application,
					   const info::Stream &info);
	~FileStream() final;

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	void RecordStart(std::vector<int32_t> selected_tracks);
	void RecordStop();

private:
	bool Start() override;
	bool Stop() override;

	std::shared_ptr<FileWriter>				_writer;

	std::shared_ptr<mon::StreamMetrics>		_stream_metrics;	

private:
	// generating temporary file path, recording file path, recording information path.
	ov::String GetOutputTempFilePath();
	ov::String GetOutputFilePath();
	ov::String GetOutputFileInfoPath();
	ov::String ConvertMacro(ov::String src);
	bool MakeDirectoryRecursive(std::string s,mode_t mode = S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

};
