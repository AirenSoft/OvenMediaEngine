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

extern "C"
{
#include <libavformat/avformat.h>
}

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
		_thread_count = 0;
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

	void SetThreadCount(int thread_count);
	int GetThreadCount();

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

	// Number of threads in encoding
	// 0: Auto
	int _thread_count;

public:
	//--------------------------------------------------------------------
	// Informal Options
	//--------------------------------------------------------------------
	void SetH264hasBframes(int32_t bframes_count);
	int32_t GetH264hasBframes() const;

private:
	int32_t _h264_has_bframes;
};


class MediaFrame
{
public:
	MediaFrame() = default;
	~MediaFrame() {
		if(_priv_data)
		{
			av_frame_unref(_priv_data);
			av_frame_free(&_priv_data);
			_priv_data = nullptr;
		}
	}

	void SetMediaType(cmn::MediaType media_type)
	{
		_media_type = media_type;
	}

	cmn::MediaType GetMediaType() const
	{
		return _media_type;
	}

	void SetTrackId(int32_t track_id)
	{
		_track_id = track_id;
	}

	int32_t GetTrackId() const
	{
		return _track_id;
	}

	int64_t GetPts() const
	{
		return _pts;
	}

	void SetPts(int64_t pts)
	{
		_pts = pts;
	}

	int64_t GetDuration() const
	{
		return _duration;
	}

	void SetDuration(int64_t duration)
	{
		_duration = duration;
	}

	void SetWidth(int32_t width)
	{
		_width = width;
	}

	int32_t GetWidth() const
	{
		return _width;
	}

	void SetHeight(int32_t height)
	{
		_height = height;
	}

	int32_t GetHeight() const
	{
		return _height;
	}

	void SetFormat(int32_t format)
	{
		_format = format;
	}

	int32_t GetFormat() const
	{
		return _format;
	}

	template <typename T>
	T GetFormat() const
	{
		return static_cast<T>(_format);
	}

	int32_t GetBytesPerSample() const
	{
		return _bytes_per_sample;
	}

	void SetBytesPerSample(int32_t bytes_per_sample)
	{
		_bytes_per_sample = bytes_per_sample;
	}

	int32_t GetNbSamples() const
	{
		return _nb_samples;
	}

	void SetNbSamples(int32_t nb_samples)
	{
		_nb_samples = nb_samples;
	}

	cmn::AudioChannel &GetChannels()
	{
		return _channels;
	}

	void SetChannels(cmn::AudioChannel channels)
	{
		_channels = channels;
	}

	void SetChannelCount(uint32_t count)
	{
		_channels.SetCount(count);
	}

	uint32_t GetChannelCount() const
	{
		return _channels.GetCounts();
	}

	int32_t GetSampleRate() const
	{
		return _sample_rate;
	}

	void SetSampleRate(int32_t sample_rate)
	{
		_sample_rate = sample_rate;
	}

	void SetFlags(int32_t flags)
	{
		_flags = flags;
	}

	int32_t GetFlags() const
	{
		return _flags;
	}

	// This function should only be called before filtering (_track_id 0, 1)
	std::shared_ptr<MediaFrame> CloneFrame()
	{
		auto frame = std::make_shared<MediaFrame>();

		if(_priv_data != nullptr){
			frame->SetPrivData(::av_frame_clone(_priv_data));
		}

		if (_media_type == cmn::MediaType::Video)
		{
			frame->SetWidth(_width);
			frame->SetHeight(_height);
			frame->SetFormat(_format);
			frame->SetPts(_pts);
			frame->SetDuration(_duration);
		}
		else if (_media_type == cmn::MediaType::Audio)
		{
			frame->SetFormat(_format);
			frame->SetBytesPerSample(_bytes_per_sample);
			frame->SetNbSamples(_nb_samples);
			frame->SetChannels(_channels);
			frame->SetSampleRate(_sample_rate);
			frame->SetPts(_pts);
			frame->SetDuration(_duration);
		}
		else
		{
			OV_ASSERT2(false);
			return nullptr;
		}
		return frame;
	}

	void SetPrivData(AVFrame* ptr) {
		_priv_data = ptr;
	}
	AVFrame* GetPrivData() const {
		return _priv_data;
	}

private:
	AVFrame *_priv_data = nullptr;

	// Data plane, Data
	cmn::MediaType _media_type = cmn::MediaType::Unknown;
	int32_t _track_id = 0;
	int64_t _pts = 0LL;
	int64_t _duration = 0LL;

	int32_t _width = 0;
	int32_t _height = 0;
	int32_t _format = 0;

	int32_t _bytes_per_sample = 0;
	int32_t _nb_samples = 0;
	cmn::AudioChannel _channels;
	int32_t _sample_rate = 0;

	int32_t _flags = 0;	 // Key, non-Key
};
