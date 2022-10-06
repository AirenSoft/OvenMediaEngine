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

		if(_priv_data) {
			_priv_data->pts = pts;
			_priv_data->pkt_dts = pts;
		}
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

	// 데이터를 빈값으로 채움
	// FillZero
	void FillZeroData()
	{
		if(!_priv_data) {
			return;
		}

		for (int i = 0; i < AV_NUM_DATA_POINTERS; i++)
		{
			if(_priv_data->linesize[i] > 0)
			{
				memset(_priv_data->data[i], 0, _priv_data->linesize[i]);
			}	
		}
	}

	// This function should only be called before filtering (_track_id 0, 1)
	std::shared_ptr<MediaFrame> CloneFrame()
	{
		auto frame = std::make_shared<MediaFrame>();

		if(_priv_data != nullptr){
			frame->SetPrivData(::av_frame_clone(_priv_data));
		}

		frame->SetMediaType(_media_type);

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
