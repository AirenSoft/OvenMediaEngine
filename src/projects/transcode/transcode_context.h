//==============================================================================
//
//  TranscodeContext
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <stdint.h>

#include <base/ovlibrary/ovlibrary.h>
#include "base/media_route/media_type.h"

class TranscodeContext
{
public:
	TranscodeContext();
	~TranscodeContext();

	//--------------------------------------------------------------------
	// Video transcoding options
	//--------------------------------------------------------------------
	void SetVideoCodecId(MediaCommonType::MediaCodecId val);
	MediaCommonType::MediaCodecId GetVideoCodecId();

	void SetVideoBitrate(int32_t val);
	int32_t GetVideoBitrate();

	void SetVideoWidth(uint32_t val);
	void SetVideoHeight(uint32_t val);

	uint32_t GetVideoWidth();
	uint32_t GetVideoHeight();

	void SetFrameRate(float val);
	float GetFrameRate();

	MediaCommonType::Timebase &GetVideoTimeBase();
	void SetVideoTimeBase(int32_t num, int32_t den);

	void SetGOP(int32_t val);
	int32_t GetGOP();

	//--------------------------------------------------------------------
	// Audio transcoding options
	//--------------------------------------------------------------------
	void SetAudioCodecId(MediaCommonType::MediaCodecId val);
	MediaCommonType::MediaCodecId GetAudioCodecId();

	void SetAudioBitrate(int32_t val);
	int32_t GetAudioBitrate();

	void SetAudioSample(MediaCommonType::AudioSample sample);
	MediaCommonType::AudioSample GetAudioSample() const;

	void SetAudioSampleRate(int32_t val);
	int32_t GetAudioSampleRate();

	MediaCommonType::Timebase &GetAudioTimeBase();

	void SetAudioTimeBase(int32_t num, int32_t den);

	void SetAudioSampleFormat(MediaCommonType::AudioSample::Format val);

	MediaCommonType::AudioChannel &GetAudioChannel();
	const MediaCommonType::AudioChannel &GetAudioChannel() const;

private:
	//--------------------------------------------------------------------
	// Video transcoding options
	//--------------------------------------------------------------------
	MediaCommonType::MediaCodecId _video_codec_id;

	// Bitrate
	int32_t _video_bitrate;

	// Resolution
	uint32_t _video_width;
	uint32_t _video_height;

	// Frame Rate
	float _video_frame_rate;

	// GOP : Group Of Picture
	int32_t _video_gop;

	// Video timebase
	MediaCommonType::Timebase _video_time_base;

	//--------------------------------------------------------------------
	// Audio transcoding options
	//--------------------------------------------------------------------
	MediaCommonType::MediaCodecId _audio_codec_id;

	// Bitrate
	int32_t _audio_bitrate;

	// Sample type
	MediaCommonType::AudioSample _audio_sample;

	// Channel
	MediaCommonType::AudioChannel _audio_channel;

	// Audio timebase
	MediaCommonType::Timebase _audio_time_base;
};

