//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <publishers/segment/segment_stream/packetizer/cmaf_chunk_writer.h>

#include "../dash/dash_packetizer.h"

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

class CmafPacketizer : public DashPacketizer
{
public:
	CmafPacketizer(const ov::String &app_name, const ov::String &stream_name,
				   PacketizerStreamType stream_type,
				   const ov::String &segment_prefix,
				   uint32_t segment_count, uint32_t segment_duration,
				   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
				   const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer);

	virtual const char *GetPacketizerName() const
	{
		return "LLDASH";
	}

	//--------------------------------------------------------------------
	// Override DashPacketizer
	//--------------------------------------------------------------------
	bool WriteVideoInit(const std::shared_ptr<ov::Data> &frame) override;
	bool WriteAudioInit(const std::shared_ptr<ov::Data> &frame) override;

	bool WriteVideoSegment() override;
	bool WriteAudioSegment() override;

	//--------------------------------------------------------------------
	// Override Packetizer
	//--------------------------------------------------------------------
	bool AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &frame) override;
	bool AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &frame) override;

protected:
	//--------------------------------------------------------------------
	// Override DashPacketizer
	//--------------------------------------------------------------------
	ov::String GetFileName(int64_t start_timestamp, cmn::MediaType media_type) const override;

	ov::String MakeJitterStatString(int64_t elapsed_time, int64_t current_time, int64_t jitter, int64_t adjusted_jitter, int64_t new_jitter_correction, int64_t video_delta, int64_t audio_delta, int64_t stream_delta) const;
	void DoJitterCorrection();
	bool UpdatePlayList() override;

private:
	std::shared_ptr<CmafChunkWriter> _video_chunk_writer = nullptr;
	std::shared_ptr<CmafChunkWriter> _audio_chunk_writer = nullptr;

	std::shared_ptr<ICmafChunkedTransfer> _chunked_transfer = nullptr;

	bool _is_first_video_frame = true;
	bool _is_first_audio_frame = true;

	int64_t _jitter_correction = 0LL;
};