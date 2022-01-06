//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcoder_context.h"

#include <unistd.h>

#include <iostream>

#include "transcoder_private.h"

TranscodeContext::TranscodeContext(bool encoder)
	: _encoder(encoder)
{
	SetTimeBase(1, 1000000);
	_hwaccel = false;
}

TranscodeContext::~TranscodeContext()
{
}

bool TranscodeContext::IsEncodingContext() const
{
	return _encoder;
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

void TranscodeContext::SetFrameRate(double val)
{
	_video_frame_rate = val;
}

double TranscodeContext::GetFrameRate()
{
	return _video_frame_rate;
}

void TranscodeContext::SetEstimateFrameRate(double val)
{
	_video_estimate_frame_rate = val;
}

double TranscodeContext::GetEstimateFrameRate()
{
	return _video_estimate_frame_rate;
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

void TranscodeContext::SetColorspace(int colorspace)
{
	_colorspace = colorspace;
}

int TranscodeContext::GetColorspace() const
{
	return _colorspace;
}

void TranscodeContext::SetHardwareAccel(bool hwaccel)
{
	_hwaccel = hwaccel;
}

bool TranscodeContext::GetHardwareAccel() const
{
	return _hwaccel;
}

void TranscodeContext::SetH264hasBframes(int32_t bframes_count)
{
	_h264_has_bframes = bframes_count;
}

int32_t TranscodeContext::GetH264hasBframes() const
{
	return _h264_has_bframes;
}

void TranscodeContext::SetAudioSamplesPerFrame(int samples)
{
	_audio_samples_per_frame = samples;
}

int TranscodeContext::GetAudioSamplesPerFrame() const
{
	return _audio_samples_per_frame;
}

void TranscodeContext::SetPreset(ov::String preset)
{
	_preset = preset;
}

ov::String TranscodeContext::GetPreset() const
{
	return _preset;
}

void TranscodeContext::SetThreadCount(int thread_count)
{
	_thread_count = thread_count;
}

int TranscodeContext::GetThreadCount()
{
	return _thread_count;
}
