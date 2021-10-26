//==============================================================================
//
//  PullProvider Base Class 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#include "provider.h"
#include "application.h"
#include "stream.h"
#include "provider_private.h"

namespace pvd
{
	PullApplication::PullApplication(const std::shared_ptr<PullProvider> &provider, const info::Application &application_info)
		: Application(provider, application_info)
	{

	}

	PullApplication::~PullApplication()
	{

	}

	bool PullApplication::Start()
	{
		_stop_collector_thread_flag = false;
		_collector_thread = std::thread(&PullApplication::WhiteElephantStreamCollector, this);
		pthread_setname_np(_collector_thread.native_handle(), "StreamCollector");
		return Application::Start();
	}

	bool PullApplication::Stop()
	{
		if(GetState() == ApplicationState::Stopped)
		{
			return true;
		}

		_stop_collector_thread_flag = true;
		if(_collector_thread.joinable())
		{
			_collector_thread.join();
		}

		return Application::Stop();
	}

	// It works only with pull provider
	void PullApplication::WhiteElephantStreamCollector()
	{
		auto no_input_timeout = GetHostInfo().GetOrigins().GetProperties().GetNoInputFailoverTimeout();
		auto unused_stream_timeout = GetHostInfo().GetOrigins().GetProperties().GetUnusedStreamDeletionTimeout();

		while(!_stop_collector_thread_flag)
		{
			// TODO (Getroot): If there is no stream, use semaphore to wait until the stream is added.
			std::shared_lock<std::shared_mutex> lock(_streams_guard, std::defer_lock);
			lock.lock();
			// Copy to prevent performance degradation due to mutex lock in loop
			auto streams = _streams;
			lock.unlock();

			for(auto const &x : streams)
			{
				auto stream = x.second;
			
				if(stream->GetState() == Stream::State::STOPPED || stream->GetState() == Stream::State::ERROR)
				{
					// Retry
					ResumeStream(stream);
				}
				else if(stream->GetState() == Stream::State::TERMINATED)
				{
					// Tried several times, but if unsuccessful, delete it completely.
					DeleteStream(stream);
				}
				else
				{
					// Check if there are streams have no any viewers
					auto stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
					if(stream_metrics != nullptr)
					{
						auto current = std::chrono::high_resolution_clock::now();
						auto elapsed_time_from_last_sent = std::chrono::duration_cast<std::chrono::milliseconds>(current - stream_metrics->GetLastSentTime()).count();
						auto elapsed_time_from_last_recv = std::chrono::duration_cast<std::chrono::milliseconds>(current - stream_metrics->GetLastRecvTime()).count();
						
						if(elapsed_time_from_last_sent > unused_stream_timeout)
						{
							logtw("%s/%s(%u) stream will be deleted because it hasn't been used for %u milliseconds", stream->GetApplicationInfo().GetName().CStr(), stream->GetName().CStr(), stream->GetId(), elapsed_time_from_last_sent);
							DeleteStream(stream);
						}
						// The stream type is pull stream, if packets do NOT arrive for more than 3 seconds, it is a seriously warning situation
						else if(elapsed_time_from_last_recv > no_input_timeout)
						{
							logtw("Stop stream %s/%s(%u) : there are no incoming packets. %d milliseconds have elapsed since the last packet was received.", 
									stream->GetApplicationInfo().GetName().CStr(), stream->GetName().CStr(), stream->GetId(), elapsed_time_from_last_recv);

							// When the stream is stopped, it tries to reconnect using the next url.
							stream->Stop();
						}
					}
				}
			}

			sleep(1);
		}
	}

	uint32_t PullApplication::GetStreamMotorId(const std::shared_ptr<PullStream> &stream)
	{
		uint32_t hash = stream->GetId() % MAX_APPLICATION_STREAM_MOTOR_COUNT;

		return hash;
	}

	std::shared_ptr<StreamMotor> PullApplication::CreateStreamMotorInternal(const std::shared_ptr<PullStream> &stream)
	{
		auto motor_id = GetStreamMotorId(stream);
		auto motor = std::make_shared<StreamMotor>(motor_id);
		motor->Start();

		std::unique_lock<std::shared_mutex> lock(_stream_motors_guard);
		_stream_motors.emplace(motor_id, motor);
		
		logti("%s application has created %u stream motor", stream->GetApplicationInfo().GetName().CStr(), motor_id);

		return motor;
	}

	std::shared_ptr<StreamMotor> PullApplication::GetStreamMotorInternal(const std::shared_ptr<PullStream> &stream)
	{
		std::shared_ptr<StreamMotor> motor = nullptr;

		auto motor_id = GetStreamMotorId(stream);

		std::shared_lock<std::shared_mutex> lock(_stream_motors_guard);
		auto it = _stream_motors.find(motor_id);
		if(it == _stream_motors.end())
		{
			logtd("Could not find stream motor : %s/%s(%u)", GetName().CStr(), stream->GetName().CStr(), stream->GetId());
			return nullptr;
		}

		motor = it->second;

		return motor;
	}

	bool PullApplication::DeleteStreamMotorInternal(const std::shared_ptr<PullStream> &stream)
	{
		auto motor = GetStreamMotorInternal(stream);
		if(motor == nullptr)
		{
			logtc("Could not find stream motor to remove stream : %s/%s(%u)", stream->GetApplicationInfo().GetName().CStr(), stream->GetName().CStr(), stream->GetId());
			return false;
		}
		
		motor->DelStream(stream);

		if(motor->GetStreamCount() == 0)
		{
			motor->Stop();
			auto motor_id = GetStreamMotorId(stream);

			std::unique_lock<std::shared_mutex> lock(_stream_motors_guard);
			_stream_motors.erase(motor_id);

			logti("%s application has deleted %u stream motor", stream->GetApplicationInfo().GetName().CStr(), motor_id);
		}

		return true;
	}

	std::shared_ptr<pvd::Stream> PullApplication::CreateStream(const ov::String &stream_name, const std::vector<ov::String> &url_list)
	{
		// Check if same stream name is exist in MediaRouter(may be created by another provider)
		if(IsExistingInboundStream(stream_name) == true)
		{
			logtw("Reject stream creation : there is already an incoming stream with the same name. (%s)", stream_name.CStr());
			return nullptr;
		}

		auto stream = CreateStream(IssueUniqueStreamId(), stream_name, url_list);
		if(stream == nullptr)
		{
			return nullptr;
		}

		std::unique_lock<std::shared_mutex> streams_lock(_streams_guard);
		
		_streams[stream->GetId()] = stream;
		auto motor = GetStreamMotorInternal(stream);
		if(motor == nullptr)
		{
			motor = CreateStreamMotorInternal(stream);
			if(motor == nullptr)
			{
				logtc("Cannot create StreamMotor : %s/%s(%u)", stream->GetApplicationInfo().GetName().CStr(), stream->GetName().CStr(), stream->GetId());
				return nullptr;
			}
		}

		streams_lock.unlock();

		// Notify first
		NotifyStreamCreated(stream);

		// And push data next
		motor->AddStream(stream);
		
		return stream;
	}

	bool PullApplication::ResumeStream(const std::shared_ptr<Stream> &stream)
	{
		auto pull_stream = std::dynamic_pointer_cast<PullStream>(stream);
		if(pull_stream == nullptr)
		{
			return false;
		}

		if(pull_stream->Resume() == false)
		{
			return false;
		}

		auto motor = GetStreamMotorInternal(pull_stream);
		if(motor == nullptr)
		{
			// Something wrong
			return false;
		}

		pull_stream->SetMsid(pull_stream->GetMsid() + 1);
		NotifyStreamUpdated(pull_stream);

		if(motor->UpdateStream(pull_stream) == false)
		{
			return false;
		}

		return true;
	}

	bool PullApplication::DeleteStream(const std::shared_ptr<Stream> &stream)
	{
		DeleteStreamMotorInternal(std::dynamic_pointer_cast<PullStream>(stream));
		return Application::DeleteStream(stream);
	}

	bool PullApplication::DeleteAllStreams()
	{
		for(const auto &x : _stream_motors)
		{
			auto motor = x.second;
			motor->Stop();
		}

		_stream_motors.clear();

		return Application::DeleteAllStreams();
	}
}