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
	// set alias
	_indicator.SetAlias(ov::String::FormatString("%s - Transcode Application Indicator", _application_info.GetName().CStr()));
}

TranscodeApplication::~TranscodeApplication()
{
	logtd("Destroyed transcode application.");
}

bool TranscodeApplication::Start()
{

	try
	{
		_kill_flag = false;
		_thread_looptask = std::thread(&TranscodeApplication::MessageLooper, this);
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;
		logte("Failed to start transcode stream thread");
		return false;
	}

	return true;
}

bool TranscodeApplication::Stop()
{
	_kill_flag = true;
	
	if (_thread_looptask.joinable())
	{
		_thread_looptask.join();
	}

	std::unique_lock<std::mutex> lock(_mutex);

	for(const auto &x : _streams)
	{
		auto stream = x.second;
		stream->Stop();
	}

	_streams.clear();

	return true;
}

bool TranscodeApplication::OnCreateStream(const std::shared_ptr<info::Stream> &stream_info)
{
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

	return stream->Push(packet);
}


bool TranscodeApplication::AppendIndicator(std::shared_ptr<TranscodeStream> stream, IndicatorQueueType queue_type)
{
	_indicator.Enqueue( std::make_shared<BufferIndicator>(stream,queue_type) );

	return true;
}

void TranscodeApplication::MessageLooper()
{
	while (!_kill_flag)
	{
		auto indicator_ref = _indicator.Dequeue(10);
		if (indicator_ref.has_value() == false)
		{
			// logti("There is no indicator"); 
			continue;
		}

		auto indicator = indicator_ref.value();

		switch(indicator->_queue_type)
		{
			case BUFFER_INDICATOR_INPUT_PACKETS:
				indicator->_stream->DoInputPackets();
				break;

			case BUFFER_INDICATOR_DECODED_FRAMES:
				indicator->_stream->DoDecodedFrames();
				break;

			case BUFFER_INDICATOR_FILTERED_FRAMES:
				indicator->_stream->DoFilteredFrames();
				break;
			default:
				break;
		}

		// logti("Append Indicator %p %d %d", indicator->_stream, indicator->_queue_type, _indicator.Size());
	}
}