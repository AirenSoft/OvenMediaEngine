//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "video_track.h"
#include "audio_track.h"

#define VALID_BITRATE_CALCULATION_THRESHOLD_MSEC (2000)

class MediaTrack : public VideoTrack, public AudioTrack
{
public:
	MediaTrack();
	MediaTrack(const MediaTrack &media_track);
	~MediaTrack();

	// Track ID
	void SetId(uint32_t id);
	uint32_t GetId() const;

	// Track Name (used for variant playlist)
	void SetName(const ov::String &name);
	ov::String GetName() const;

	// Video Type Settings
	void SetMediaType(cmn::MediaType type);
	cmn::MediaType GetMediaType() const;

	// Codec Settings
	void SetCodecId(cmn::MediaCodecId id);
	cmn::MediaCodecId GetCodecId() const;

	// Origin bitstream foramt
	void SetOriginBitstream(cmn::BitstreamFormat format);
	cmn::BitstreamFormat GetOriginBitstream() const;

	// Timebase Settings
	const cmn::Timebase &GetTimeBase() const;
	void SetTimeBase(int32_t num, int32_t den);
	void SetTimeBase(const cmn::Timebase &time_base);

	// Bitrate Settings
	void SetBitrate(int32_t bitrate);
	int32_t GetBitrate() const;

	// Frame Time Settings
	void SetStartFrameTime(int64_t time);
	int64_t GetStartFrameTime() const;

	void SetLastFrameTime(int64_t time);
	int64_t GetLastFrameTime() const;

	// Bypass Settings
	void SetBypass(bool flag);
	bool IsBypass();

	ov::String GetInfoString();
	bool IsValid();

	// Define extradata by codec
	//  H264 : AVCDecoderConfigurationRecord
	//  H265 : HEVCDecoderConfiguratinRecord(TODO)
	//  AAC : AACSpecificConfig
	//  VP8 : No plan
	//  OPUS : No plan
	void SetCodecExtradata(const std::shared_ptr<ov::Data> &codec_extradata);
	const std::shared_ptr<ov::Data> &GetCodecExtradata() const;
	std::shared_ptr<ov::Data> &GetCodecExtradata();

	// For statistics
	void OnFrameAdded(uint64_t bytes);

private:
	bool _is_valid = false;

	uint32_t _id;
	ov::String _name;

	cmn::MediaCodecId _codec_id;
	cmn::BitstreamFormat _origin_bitstream_format = cmn::BitstreamFormat::Unknown;
	cmn::MediaType _media_type;
	cmn::Timebase _time_base;
	int32_t _bitrate;

	bool	_byass;

	// Time of start frame(packet)
	int64_t _start_frame_time;

	// Time of last frame(packet)
	int64_t _last_frame_time;

	std::shared_ptr<ov::Data> _codec_extradata = nullptr;

	// Statistics

	// First frame received time
	ov::StopWatch _clock_from_first_frame_received;

	uint64_t _total_frame_count = 0;
	uint64_t _total_frame_bytes = 0;
};
