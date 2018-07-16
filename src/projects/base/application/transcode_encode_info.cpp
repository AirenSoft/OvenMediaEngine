//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_encode_info.h"

TranscodeEncodeInfo::TranscodeEncodeInfo()
{
}

TranscodeEncodeInfo::~TranscodeEncodeInfo()
{
}

const bool TranscodeEncodeInfo::GetActive() const noexcept
{
	return _active;
}

void TranscodeEncodeInfo::SetActive(bool active)
{
	_active = active;
}

const ov::String TranscodeEncodeInfo::GetName() const noexcept
{
	return _name;
}

void TranscodeEncodeInfo::SetName(ov::String name)
{
	_name = name;
}

const ov::String TranscodeEncodeInfo::GetStreamName() const noexcept
{
	return _stream_name;
}

void TranscodeEncodeInfo::SetStreamName(ov::String stream_name)
{
	_stream_name = stream_name;
}

const ov::String TranscodeEncodeInfo::GetContainer() const noexcept
{
	return _container;
}

void TranscodeEncodeInfo::SetContainer(ov::String container)
{
	_container = container;
}

std::shared_ptr<const TranscodeVideoInfo> TranscodeEncodeInfo::GetVideo() const noexcept
{
	return _video;
}

void TranscodeEncodeInfo::SetVideo(std::shared_ptr<TranscodeVideoInfo> video)
{
	_video = video;
}

std::shared_ptr<const TranscodeAudioInfo> TranscodeEncodeInfo::GetAudio() const noexcept
{
	return _audio;
}

void TranscodeEncodeInfo::SetAudio(std::shared_ptr<TranscodeAudioInfo> audio)
{
	_audio = audio;
}

ov::String TranscodeEncodeInfo::ToString() const
{
	ov::String result = ov::String::FormatString("{\"active\": %d, \"name\": \"%s\", \"stream_name\": \"%s\", \"container\": \"%s\"", _active, _name.CStr(), _stream_name.CStr(), _container.CStr());

	if(_video != nullptr)
	{
		result.AppendFormat(", \"video\": [%s]", _video->ToString().CStr());
	}

	if(_audio != nullptr)
	{
		result.AppendFormat(", \"audio\": [%s]", _audio->ToString().CStr());
	}

	result.Append("}");

	return result;
}