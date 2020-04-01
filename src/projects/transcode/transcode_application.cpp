//==============================================================================
//
//  TranscodeApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <iostream>
#include <unistd.h>

#include "transcode_application.h"

#define OV_LOG_TAG "TranscodeApplication"

std::shared_ptr<TranscodeApplication> TranscodeApplication::Create(const info::Application &application_info)
{
	auto instance = std::make_shared<TranscodeApplication>(application_info);
	instance->Start();
	return instance;
}

TranscodeApplication::TranscodeApplication(const info::Application &application_info)
	: _application_info(application_info)
{
}

TranscodeApplication::~TranscodeApplication()
{
	logtd("Destroyed transcode application.");
}

bool TranscodeApplication::Start()
{
	logti("Transcoder has started [%s] application", _application_info.GetName().CStr());
	return true;
}

bool TranscodeApplication::Stop()
{
	logti("Transcoder has stopped [%s] application", _application_info.GetName().CStr());
	return true;
}

bool TranscodeApplication::OnCreateStream(const std::shared_ptr<info::Stream> &stream_info)
{
	logtd("OnCreateStream. name(%s), id(%d)", stream_info->GetName().CStr(), stream_info->GetId());

	std::unique_lock<std::mutex> lock(_mutex);

	auto stream = std::make_shared<TranscodeStream>(_application_info, stream_info, this);
	if (stream == nullptr)
	{
		return false;
	}

	if(stream->Start() == false)
	{
		return false;
	}

	_streams.insert(std::make_pair(stream_info->GetId(), stream));

	return true;
}

bool TranscodeApplication::OnDeleteStream(const std::shared_ptr<info::Stream> &stream_info)
{
	logtd("OnDeleteStream. name(%s), id(%d)", stream_info->GetName().CStr(), stream_info->GetId());
	
	std::unique_lock<std::mutex> lock(_mutex);

	auto stream_bucket = _streams.find(stream_info->GetId());

	if (stream_bucket == _streams.end())
	{
		return false;
	}

	auto stream = stream_bucket->second;

	stream->Stop();

	_streams.erase(stream_info->GetId());

	return true;
}


bool TranscodeApplication::OnSendFrame(const std::shared_ptr<info::Stream> &stream_info, const std::shared_ptr<MediaPacket> &packet)
{
	std::unique_lock<std::mutex> lock(_mutex);

	auto stream_bucket = _streams.find(stream_info->GetId());

	if (stream_bucket == _streams.end())
	{
		return false;
	}

	auto stream = stream_bucket->second;

	return stream->Push(std::move(packet));
}
