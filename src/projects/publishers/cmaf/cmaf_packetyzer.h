//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <segment_stream/packetyzer/cmaf_chunk_writer.h>
#include "../dash/dash_packetyzer.h"

class ICmafChunkedTransfer
{
public:
	// This callback will be called when each frame is received
	virtual void OnCmafChunkDataPush(const ov::String &app_name, const ov::String &stream_name,
									 const ov::String &file_name,
									 bool is_video,
									 std::shared_ptr<ov::Data> &chunk_data) = 0;

	virtual void OnCmafChunkedComplete(const ov::String &app_name, const ov::String &stream_name,
									   const ov::String &file_name,
									   bool is_video) = 0;
};

class CmafPacketyzer : public DashPacketyzer
{
public:
	CmafPacketyzer(const ov::String &app_name, const ov::String &stream_name,
				   PacketyzerStreamType stream_type,
				   const ov::String &segment_prefix,
				   uint32_t segment_count, uint32_t segment_duration,
				   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
				   const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer);

	//--------------------------------------------------------------------
	// Override DashPacketyzer
	//--------------------------------------------------------------------
	bool WriteVideoInit(const std::shared_ptr<ov::Data> &frame) override;
	bool WriteAudioInit(const std::shared_ptr<ov::Data> &frame) override;

	bool WriteVideoSegment() override;
	bool WriteAudioSegment() override;

	//--------------------------------------------------------------------
	// Override Packetyzer
	//--------------------------------------------------------------------
	bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame) override;
	bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame) override;

	bool GetPlayList(ov::String &play_list) override;

protected:
	ov::String GetFileName(int64_t start_timestamp, common::MediaType media_type) const override;

	bool UpdatePlayList() override;

private:
	std::unique_ptr<CmafChunkWriter> _video_chunk_writer = nullptr;
	std::unique_ptr<CmafChunkWriter> _audio_chunk_writer = nullptr;

	std::shared_ptr<ICmafChunkedTransfer> _chunked_transfer = nullptr;
};