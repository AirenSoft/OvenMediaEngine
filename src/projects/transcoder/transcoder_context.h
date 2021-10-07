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
	TranscodeContext(bool encoder);
	~TranscodeContext();

	// Video
	TranscodeContext(
		bool encoder,
		cmn::MediaCodecId codec_id,
		int32_t bitrate,
		uint32_t width,
		uint32_t height,
		float frame_rate,
		int colorspace)
		: _encoder(encoder),
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
		_preset = "";
	}

	// Audio
	TranscodeContext(
		bool encoder,
		cmn::MediaCodecId codec_id,
		int32_t bitrate,
		int32_t sample)
		: _encoder(encoder),
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
	bool IsEncodingContext() const;

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
	int GetColorspace() const;

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
	bool GetHardwareAccel() const;

	void SetAudioSamplesPerFrame(int nbsamples);
	int GetAudioSamplesPerFrame() const;

	void SetPreset(ov::String preset);
	ov::String GetPreset() const;

private:
	// Context type
	//    true = this context will be used for encoding
	//    false = this context will be used for decoding
	bool _encoder = false;

	// Codec ID
	cmn::MediaCodecId _codec_id;

	// Media Type
	cmn::MediaType _media_type;

	// Bitrate
	int32_t _bitrate;

	// Video Timebase
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

	// Audio Sample Format
	cmn::AudioSample _audio_sample;

	// Audio Channel
	cmn::AudioChannel _audio_channel;

	// Sample Count per frame
	int _audio_samples_per_frame;

	// Hardware accelerator
	bool _hwaccel;

	ov::String _preset;

public:
	//--------------------------------------------------------------------
	// Informal Options
	//--------------------------------------------------------------------
	void SetH264hasBframes(int32_t bframes_count);
	int32_t GetH264hasBframes() const;

private:
	int32_t _h264_has_bframes;
};
