//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <unistd.h>
#include <iostream>

#include "transcode_context.h"
#include "transcode_private.h"

TranscodeContext::TranscodeContext(bool is_encoding_context)
	: _is_encoding_context(is_encoding_context)
{
	SetTimeBase(1, 1000000);
}

TranscodeContext::~TranscodeContext()
{
}

void TranscodeContext::SetCodecId(cmn::MediaCodecId val)
{
	_codec_id = val;
}

cmn::MediaCodecId TranscodeContext::GetCodecId() const
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

void TranscodeContext::SetAudioSample(cmn::AudioSample sample)
{
	_audio_sample = sample;
}

cmn::AudioSample TranscodeContext::GetAudioSample() const
{
	return _audio_sample;
}

void TranscodeContext::SetAudioSampleRate(int32_t val)
{
	_audio_sample.SetRate((cmn::AudioSample::Rate)val);
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

const cmn::Timebase &TranscodeContext::GetTimeBase() const
{
	return _time_base;
}

void TranscodeContext::SetTimeBase(const cmn::Timebase &timebase)
{
	_time_base = timebase;
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

void TranscodeContext::SetAudioSampleFormat(cmn::AudioSample::Format val)
{
	_audio_sample.SetFormat(val);
}

void TranscodeContext::SetAudioChannel(cmn::AudioChannel channel)
{
	_audio_channel = channel;
}

cmn::AudioChannel &TranscodeContext::GetAudioChannel()
{
	return _audio_channel;
}


const cmn::AudioChannel &TranscodeContext::GetAudioChannel() const
{
	return _audio_channel;
}

cmn::MediaType TranscodeContext::GetMediaType() const
{
	return _media_type;
}
