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

using namespace MediaCommonType;

class TranscodeContext
{
public:
	TranscodeContext();
	~TranscodeContext();

	// Video
	TranscodeContext(
		ov::String &stream_name,
		MediaCodecId codec_id,
		int32_t bitrate,
		uint32_t width,
		uint32_t height,
		float frame_rate) : _stream_name(stream_name),
							_codec_id(codec_id),
					        _bitrate(bitrate),
					        _video_width(width),
					        _video_height(height),
					        _video_frame_rate(frame_rate)
	{
		_media_type = MediaType::Video;
		_time_base.Set(1, 1000000);
		_video_gop = 30;
	}

	// Audio
	TranscodeContext(
		ov::String &stream_name,
		MediaCodecId codec_id,
		int32_t bitrate,
		int32_t sample) : _stream_name(stream_name),
						  _codec_id(codec_id),
	                      _bitrate(bitrate)
	{
		_media_type = MediaType::Audio;
		_time_base.Set(1, 1000000);
		_audio_sample.SetRate((AudioSample::Rate)sample);
		_audio_sample.SetFormat(AudioSample::Format::S16);
		_audio_channel.SetLayout(AudioChannel::Layout::LayoutStereo);
	}

	//--------------------------------------------------------------------
	// Video transcoding options
	//--------------------------------------------------------------------
	void SetCodecId(MediaCodecId val);
	MediaCodecId GetCodecId();

	void SetBitrate(int32_t val);
	int32_t GetBitrate();

	Timebase &GetTimeBase();
	void SetTimeBase(int32_t num, int32_t den);

	void SetVideoWidth(uint32_t val);
	void SetVideoHeight(uint32_t val);

	uint32_t GetVideoWidth();
	uint32_t GetVideoHeight();

	void SetGOP(int32_t val);
	int32_t GetGOP();

	void SetFrameRate(float val);
	float GetFrameRate();

	void SetAudioSample(AudioSample sample);
	AudioSample GetAudioSample() const;

	void SetAudioSampleRate(int32_t val);
	int32_t GetAudioSampleRate();

	void SetAudioSampleFormat(AudioSample::Format val);

	AudioChannel &GetAudioChannel();
	const AudioChannel &GetAudioChannel() const;

	MediaType GetMediaType() const;

	ov::String GetStreamName() const;

private:
	//--------------------------------------------------------------------
	// Video transcoding options
	//--------------------------------------------------------------------
	MediaCodecId _codec_id;

	// Bitrate
	int32_t _bitrate;

	// Video timebase
	Timebase _time_base;

	// Resolution
	uint32_t _video_width;
	uint32_t _video_height;

	// Frame Rate
	float _video_frame_rate;

	// GOP : Group Of Picture
	int32_t _video_gop;

	MediaType _media_type;

	// Sample type
	AudioSample _audio_sample;

	// Channel
	AudioChannel _audio_channel;

	// Linked stream name
	ov::String _stream_name;
};

