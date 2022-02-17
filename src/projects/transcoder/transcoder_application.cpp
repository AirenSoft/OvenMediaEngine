//==============================================================================
//
//  TranscodeApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcoder_application.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

#include <unistd.h>
#include <iostream>

#define MIN_APPLICATION_WORKER_COUNT 1
#define MAX_APPLICATION_WORKER_COUNT 72
#define MAX_QUEUE_SIZE 100

std::shared_ptr<TranscodeApplication> TranscodeApplication::Create(const info::Application &application_info)
{
	auto instance = std::make_shared<TranscodeApplication>(application_info);
	instance->Start();
	return instance;
}

TranscodeApplication::TranscodeApplication(const info::Application &application_info)
	: _application_info(application_info)
{
	logti("Created transcoder application. app(%s)", application_info.GetName().CStr());
}

TranscodeApplication::~TranscodeApplication()
{
	logti("Transcoder application has been destroyed. app(%s)", _application_info.GetName().CStr());
}

bool TranscodeApplication::Start()
{
#if SUPPORT_HWACCELS
	if(_application_info.GetConfig().GetOutputProfiles().IsHardwareAcceleration() == true)
	{
		if (TranscodeGPU::GetInstance()->Initialze() == false)
		{
			logtw("There is no supported hardware accelerator");
		}
	}
#endif	
	return true;
}

bool TranscodeApplication::Stop()
{
	for (const auto &it : _streams)
	{
		auto stream = it.second;
		stream->Stop();
	}
	_streams.clear();

	logtd("Transcoder application has been stopped. app(%s)", _application_info.GetName().CStr());

	return true;
}

bool TranscodeApplication::OnStreamCreated(const std::shared_ptr<info::Stream> &stream_info)
{
	std::unique_lock<std::mutex> lock(_mutex);

	auto stream = std::make_shared<TranscoderStream>(_application_info, stream_info, this);
	if (stream == nullptr)
	{
		return false;
	}

	if (stream->Start() == false)
	{
		return false;
	}

	_streams.insert(std::make_pair(stream_info->GetId(), stream));

	return true;
}

bool TranscodeApplication::OnStreamDeleted(const std::shared_ptr<info::Stream> &stream_info)
{
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

bool TranscodeApplication::OnStreamPrepared(const std::shared_ptr<info::Stream> &stream_info)
{
	std::unique_lock<std::mutex> lock(_mutex);

	// logte("Called OnStreamParsed. *Please delete this log after checking.*");

	auto stream_bucket = _streams.find(stream_info->GetId());
	if (stream_bucket == _streams.end())
	{
		return false;
	}

	auto stream = stream_bucket->second;
	if (stream->Prepare() == false)
	{
		return false;
	}

	return true;
}

bool TranscodeApplication::OnStreamUpdated(const std::shared_ptr<info::Stream> &stream_info)
{
	std::unique_lock<std::mutex> lock(_mutex);

	// logte("Called OnStreamUpdated. *Please delete this log after checking.*");

	auto stream_bucket = _streams.find(stream_info->GetId());
	if (stream_bucket == _streams.end())
	{
		return false;
	}

	auto stream = stream_bucket->second;
	if (stream->Update(stream_info) == false)
	{
		return false;
	}

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

	return stream->Push(packet);
}
