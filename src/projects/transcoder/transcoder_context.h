//==============================================================================
//
//  TranscodeContext
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <stdint.h>

#include "base/mediarouter/media_type.h"

class TranscodeContext
{
public:
	TranscodeContext(bool is_encoding_context);
	~TranscodeContext();

	// Video
	TranscodeContext(
		bool is_encoding_context,
		cmn::MediaCodecId codec_id,
		int32_t bitrate,
		uint32_t width,
		uint32_t height,
		float frame_rate,
		int colorspace)
		: _is_encoding_context(is_encoding_context),
		  _codec_id(codec_id),
		  _bitrate(bitrate),
		  _video_width(width),
		  _video_height(height),
		  _video_frame_rate(frame_rate),
		  _colorspace(colorspace)
	{
		_media_type = cmn::MediaType::Video;
		_time_base.Set(1, 90000);
		_video_gop = 30;
		_h264_has_bframes = 0;
	}

	// Audio
	TranscodeContext(
		bool is_encoding_context,
		cmn::MediaCodecId codec_id,
		int32_t bitrate,
		int32_t sample)
		: _is_encoding_context(is_encoding_context),
		  _codec_id(codec_id),
		  _bitrate(bitrate)
	{
		_media_type = cmn::MediaType::Audio;
		_time_base.Set(1, sample);
		_audio_sample.SetRate((cmn::AudioSample::Rate)sample);
		_audio_sample.SetFormat(cmn::AudioSample::Format::S16);
		_audio_channel.SetLayout(cmn::AudioChannel::Layout::LayoutStereo);
	}

	// To determine whether this transcode context contains encoding/decoding information
	bool IsEncodingContext() const
	{
		return _is_encoding_context;
	}

	//--------------------------------------------------------------------
	// Video transcoding options
	//--------------------------------------------------------------------
	void SetCodecId(cmn::MediaCodecId val);
	cmn::MediaCodecId GetCodecId() const;

	void SetBitrate(int32_t val);
	int32_t GetBitrate();

	const cmn::Timebase &GetTimeBase() const;
	void SetTimeBase(const cmn::Timebase &timebase);
	void SetTimeBase(int32_t num, int32_t den);

	void SetVideoWidth(uint32_t val);
	void SetVideoHeight(uint32_t val);

	uint32_t GetVideoWidth();
	uint32_t GetVideoHeight();

	void SetColorspace(int colorspace);
	int GetColorspace();

	void SetGOP(int32_t val);
	int32_t GetGOP();

	void SetFrameRate(double val);
	double GetFrameRate();

	void SetEstimateFrameRate(double val);
	double GetEstimateFrameRate();

	void SetAudioSample(cmn::AudioSample sample);
	cmn::AudioSample GetAudioSample() const;

	void SetAudioSampleRate(int32_t val);
	int32_t GetAudioSampleRate();

	void SetAudioSampleFormat(cmn::AudioSample::Format val);

	void SetAudioChannel(cmn::AudioChannel channel);
	cmn::AudioChannel &GetAudioChannel();
	const cmn::AudioChannel &GetAudioChannel() const;

	cmn::MediaType GetMediaType() const;

	void SetHardwareAccel(bool hwaccel);
	bool GetHardwareAccel();

	void SetAudioSamplesPerFrame(int nbsamples);
	int GetAudioSamplesPerFrame();

private:
	// Context type
	//    true = this context will be used for encoding
	//    false = this context will be used for decoding
	bool _is_encoding_context = false;

	//--------------------------------------------------------------------
	// Video transcoding options
	//--------------------------------------------------------------------
	cmn::MediaCodecId _codec_id;

	// Bitrate
	int32_t _bitrate;

	// Video timebase
	cmn::Timebase _time_base;

	// Resolution
	uint32_t _video_width;
	uint32_t _video_height;

	// Frame Rate
	double _video_frame_rate;
	double _video_estimate_frame_rate;

	// Colorspace
	// - This variable is temporarily used in the Pixel Format defined by FFMPEG.
	int _colorspace;

	// GOP : Group Of Picture
	int32_t _video_gop;

	cmn::MediaType _media_type;

	// Sample type
	cmn::AudioSample _audio_sample;

	// Channel
	cmn::AudioChannel _audio_channel;

	int _audio_samples_per_frame;
	// Hardware accelerator
	bool _hwaccel;

public:
	//--------------------------------------------------------------------
	// Informal Options
	//--------------------------------------------------------------------
	void SetH264hasBframes(int32_t bframes_count);
	int32_t GetH264hasBframes();

private:
	int32_t _h264_has_bframes;
};
