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

class MediaTrack : public VideoTrack, public AudioTrack
{
public:
	MediaTrack();
	MediaTrack(const MediaTrack &media_track);
	~MediaTrack();

	// Track ID
	void SetId(uint32_t id);
	uint32_t GetId() const;

	// Video Type Settings
	void SetMediaType(cmn::MediaType type);
	cmn::MediaType GetMediaType() const;

	// Codec Settings
	void SetCodecId(cmn::MediaCodecId id);
	cmn::MediaCodecId GetCodecId() const;

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

	// Define extradata by codec
	//  H264 : AVCDecoderConfigurationRecord
	//  H265 : HEVCDecoderConfiguratinRecord(TODO)
	//  AAC : AACSpecificConfig
	//  VP8 : No plan
	//  OPUS : No plan
	void SetCodecExtradata(std::vector<uint8_t> codec_extradata);
	const std::vector<uint8_t> &GetCodecExtradata() const;
	std::vector<uint8_t> &GetCodecExtradata();

	ov::String GetInfoString();

	bool IsValidity();

private:
	uint32_t _id;

	cmn::MediaCodecId _codec_id;
	cmn::MediaType _media_type;
	cmn::Timebase _time_base;
	int32_t _bitrate;

	bool	_byass;

	// Time of start frame(packet)
	int64_t _start_frame_time;

	// Time of last frame(packet)
	int64_t _last_frame_time;

	std::vector<uint8_t> _codec_extradata;
};
