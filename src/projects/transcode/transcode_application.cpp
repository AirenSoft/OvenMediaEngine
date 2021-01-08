//==============================================================================
//
//  TranscodeApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcode_application.h"
#include "transcode_private.h"
#include <unistd.h>

#include <iostream>

#define MIN_APPLICATION_WORKER_COUNT 1
#define MAX_APPLICATION_WORKER_COUNT 72

std::shared_ptr<TranscodeApplication> TranscodeApplication::Create(const info::Application &application_info)
{
	auto instance = std::make_shared<TranscodeApplication>(application_info);
	instance->Start();
	return instance;
}

TranscodeApplication::TranscodeApplication(const info::Application &application_info)
	: _application_info(application_info)
{
	_max_worker_thread_count = _application_info.GetConfig().GetPublishers().GetStreamLoadBalancingThreadCount();

	if (_max_worker_thread_count < MIN_APPLICATION_WORKER_COUNT)
	{
		_max_worker_thread_count = MIN_APPLICATION_WORKER_COUNT;
	}
	if (_max_worker_thread_count > MAX_APPLICATION_WORKER_COUNT)
	{
		_max_worker_thread_count = MAX_APPLICATION_WORKER_COUNT;
	}

	for (uint32_t worker_id = 0; worker_id < _max_worker_thread_count; worker_id++)
	{
		_indicators.push_back(std::make_shared<ov::Queue<std::shared_ptr<BufferIndicator>>>(
			ov::String::FormatString("Transcoder application indicator. app(%s) (%d/%d)", application_info.GetName().CStr(), worker_id, _max_worker_thread_count),
			1024));
	}

	logtd("Created transcoder application. app(%s)", application_info.GetName().CStr());
}

TranscodeApplication::~TranscodeApplication()
{

	logtd("Transcoder application has been destroyed. app(%s)", _application_info.GetName().CStr());
}

bool TranscodeApplication::Start()
{
	_kill_flag = false;

	std::unique_lock<std::mutex> lock(_mutex);

	for (uint32_t worker_id = 0; worker_id < _max_worker_thread_count; worker_id++)
	{
		try
		{
			auto worker_t = std::thread(&TranscodeApplication::WorkerThread, this, worker_id);
			pthread_setname_np(worker_t.native_handle(), "TcAppWorker");
			_worker_threads.push_back(std::move(worker_t));
		}
		catch (const std::system_error &e)
		{
			_kill_flag = true;

			logte("Failed to start transcoder application worker thread. app(%s)", _application_info.GetName().CStr());

			return false;
		}
	}

	return true;
}

bool TranscodeApplication::Stop()
{
	_kill_flag = true;

	std::unique_lock<std::mutex> lock(_mutex);

	for (auto &indicator : _indicators)
	{
		indicator->Stop();
		indicator->Clear();
	}

	for (auto &worker : _worker_threads)
	{
		if (worker.joinable())
		{
			worker.join();
		}
	}

	for (const auto &it : _streams)
	{
		auto stream = it.second;
		stream->Stop();
	}

	_indicators.clear();
	_worker_threads.clear();
	_streams.clear();

	logtd("Transcoder application has been stopped. app(%s)", _application_info.GetName().CStr());

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

	if (stream->Start() == false)
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
	_indicators[stream->GetStreamId() % _max_worker_thread_count]->Enqueue(std::make_shared<BufferIndicator>(stream, queue_type));

	return true;
}

void TranscodeApplication::WorkerThread(uint32_t worker_id)
{
	while (!_kill_flag)
	{
		auto indicator_ref = _indicators[worker_id]->Dequeue(ov::Infinite);
		if (indicator_ref.has_value() == false)
		{
			// No indicator
			continue;
		}

		auto indicator = indicator_ref.value();

		switch (indicator->_queue_type)
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
	}
}