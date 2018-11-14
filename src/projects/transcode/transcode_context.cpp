//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <iostream>
#include <unistd.h>

#include "transcode_context.h"

#define OV_LOG_TAG "TranscodeContext"

TranscodeContext::TranscodeContext()
{
	SetTimeBase(1, 1000000);
}

TranscodeContext::~TranscodeContext()
{
}

void TranscodeContext::SetCodecId(common::MediaCodecId val)
{
	_codec_id = val;
}

common::MediaCodecId TranscodeContext::GetCodecId()
{
	return _codec_id;
}

void TranscodeContext::SetBitrate(int32_t val)
{
	_bitrate = val;
}

int32_t TranscodeContext::GetBitrate()
{
	return _bitrate;
}

void TranscodeContext::SetAudioSample(common::AudioSample sample)
{
	_audio_sample = sample;
}

common::AudioSample TranscodeContext::GetAudioSample() const
{
	return _audio_sample;
}

void TranscodeContext::SetAudioSampleRate(int32_t val)
{
	_audio_sample.SetRate((common::AudioSample::Rate)val);
	// _audio_sample_rate = val;
}

int32_t TranscodeContext::GetAudioSampleRate()
{
	return (int32_t)_audio_sample.GetRate();
}

void TranscodeContext::SetVideoWidth(uint32_t val)
{
	_video_width = val;
}

void TranscodeContext::SetVideoHeight(uint32_t val)
{
	_video_height = val;
}

uint32_t TranscodeContext::GetVideoWidth()
{
	return _video_width;
}

uint32_t TranscodeContext::GetVideoHeight()
{
	return _video_height;
}

void TranscodeContext::SetFrameRate(float val)
{
	_video_frame_rate = val;
}

float TranscodeContext::GetFrameRate()
{
	return _video_frame_rate;
}

common::Timebase &TranscodeContext::GetTimeBase()
{
	return _time_base;
}

void TranscodeContext::SetTimeBase(int32_t num, int32_t den)
{
	_time_base.Set(num, den);
}

void TranscodeContext::SetGOP(int32_t val)
{
	_video_gop = val;
}

int32_t TranscodeContext::GetGOP()
{
	return _video_gop;
}

void TranscodeContext::SetAudioSampleFormat(common::AudioSample::Format val)
{
	_audio_sample.SetFormat(val);
}

common::AudioChannel &TranscodeContext::GetAudioChannel()
{
	return _audio_channel;
}

const common::AudioChannel &TranscodeContext::GetAudioChannel() const
{
	return _audio_channel;
}

common::MediaType TranscodeContext::GetMediaType() const
{
	return _media_type;
}

