//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_audio_info.h"

TranscodeAudioInfo::TranscodeAudioInfo()
{
}

TranscodeAudioInfo::~TranscodeAudioInfo()
{
}

const bool TranscodeAudioInfo::GetActive() const noexcept
{
	return _active;
}

void TranscodeAudioInfo::SetActive(bool active)
{
	_active = active;
}

const ov::String TranscodeAudioInfo::GetCodec() const noexcept
{
	return _codec;
}

const MediaCodecId TranscodeAudioInfo::GetCodecId() const noexcept
{
	if(_codec.UpperCaseString() == "AAC")
	{
		return MediaCodecId::Aac;
	}
	else if(_codec.UpperCaseString() == "OPUS")
	{
		return MediaCodecId::Opus;
	}
	return MediaCodecId::None;
}

void TranscodeAudioInfo::SetCodec(ov::String codec)
{
	_codec = codec;
}

const int TranscodeAudioInfo::GetBitrate() const noexcept
{
	return _bitrate;
}

void TranscodeAudioInfo::SetBitrate(int bitrate)
{
	_bitrate = bitrate;
}

const int TranscodeAudioInfo::GetSamplerate() const noexcept
{
	return _samplerate;
}

void TranscodeAudioInfo::SetSamplerate(int samplerate)
{
	_samplerate = samplerate;
}

const int TranscodeAudioInfo::GetChannel() const noexcept
{
	return _channel;
}

void TranscodeAudioInfo::SetChannel(int channel)
{
	_channel = channel;
}

ov::String TranscodeAudioInfo::ToString() const
{
	return ov::String::FormatString("{\"active\": %d, \"codec\": \"%s\", \"bitrate\": %d, \"samplerate\": %d, \"channel\": %d}",
	                                _active, _codec.CStr(), _bitrate, _samplerate, _channel);
}
