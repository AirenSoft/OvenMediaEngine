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

	// Video
	TranscodeContext(
		common::MediaCodecId codec_id,
		int32_t bitrate,
		uint32_t width,
		uint32_t height,
		float frame_rate) : _codec_id(codec_id),
	                        _bitrate(bitrate),
	                        _video_width(width),
	                        _video_height(height),
	                        _video_frame_rate(frame_rate)
	{
		_media_type = common::MediaType::Video;
		_time_base.Set(1, 1000000);
		_video_gop = 30;
	}

	// Audio
	TranscodeContext(
		common::MediaCodecId codec_id,
		int32_t bitrate,
		int32_t sample) : _codec_id(codec_id),
	                      _bitrate(bitrate)
	{
		_media_type = common::MediaType::Audio;
		_time_base.Set(1, 1000000);
		_audio_sample.SetRate((common::AudioSample::Rate)sample);
		_audio_sample.SetFormat(common::AudioSample::Format::S16);
		_audio_channel.SetLayout(common::AudioChannel::Layout::LayoutStereo);
	}

	//--------------------------------------------------------------------
	// Video transcoding options
	//--------------------------------------------------------------------
	void SetCodecId(common::MediaCodecId val);
	common::MediaCodecId GetCodecId();

	void SetBitrate(int32_t val);
	int32_t GetBitrate();

	common::Timebase &GetTimeBase();
	void SetTimeBase(int32_t num, int32_t den);

	void SetVideoWidth(uint32_t val);
	void SetVideoHeight(uint32_t val);

	uint32_t GetVideoWidth();
	uint32_t GetVideoHeight();

	void SetGOP(int32_t val);
	int32_t GetGOP();

	void SetFrameRate(float val);
	float GetFrameRate();

	void SetAudioSample(common::AudioSample sample);
	common::AudioSample GetAudioSample() const;

	void SetAudioSampleRate(int32_t val);
	int32_t GetAudioSampleRate();

	void SetAudioSampleFormat(common::AudioSample::Format val);

	common::AudioChannel &GetAudioChannel();
	const common::AudioChannel &GetAudioChannel() const;

	common::MediaType GetMediaType() const;

private:
	//--------------------------------------------------------------------
	// Video transcoding options
	//--------------------------------------------------------------------
	common::MediaCodecId _codec_id;

	// Bitrate
	int32_t _bitrate;

	// Video timebase
	common::Timebase _time_base;

	// Resolution
	uint32_t _video_width;
	uint32_t _video_height;

	// Frame Rate
	float _video_frame_rate;

	// GOP : Group Of Picture
	int32_t _video_gop;

	common::MediaType _media_type;

	// Sample type
	common::AudioSample _audio_sample;

	// Channel
	common::AudioChannel _audio_channel;
};

