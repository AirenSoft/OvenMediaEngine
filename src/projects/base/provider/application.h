//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "config/config.h"

#include "base/common_types.h"
#include "base/ovlibrary/ovlibrary.h"

#include "base/media_route/media_route_application_connector.h"
#include "stream.h"

#include <shared_mutex>

//TODO(Dimiden): It has to be moved to configuration
#define MAX_STREAM_MOTOR_COUNT					10
#define MAX_UNUSED_STREAM_AVAILABLE_TIME_SEC	60
#define MAX_EPOLL_EVENTS						1024
#define EPOLL_TIMEOUT_MSEC						100
namespace pvd
{
	// StreamMotor is a thread for pull provider stream that calls the stream's ProcessMedia function periodically
	class StreamMotor
	{
	public:
		StreamMotor(uint32_t id);

		uint32_t GetId();
		uint32_t GetStreamCount();

		bool Start();
		bool Stop();

		bool AddStream(const std::shared_ptr<Stream> &stream);
		bool DelStream(const std::shared_ptr<Stream> &stream);

	private:
		bool AddStreamToEpoll(const std::shared_ptr<Stream> &stream);
		bool DelStreamFromEpoll(const std::shared_ptr<Stream> &stream);

		void WorkerThread();

		uint32_t _id;

		int _epoll_fd;

		bool _stop_thread_flag;
		std::thread _thread;
		std::shared_mutex _streams_map_guard;
		std::map<uint32_t, std::shared_ptr<Stream>> _streams;
	};

	class Provider;
	class Application : public info::Application, public MediaRouteApplicationConnector
	{
		enum class ApplicationState : int8_t
		{
			Idle,
			Started,
			Stopped,
			Error
		};

	public:
		virtual bool Start();
		virtual bool Stop();

		const std::shared_ptr<Stream> GetStreamById(uint32_t stream_id);
		const std::shared_ptr<Stream> GetStreamByName(ov::String stream_name);

		uint32_t 	IssueUniqueStreamId();

		// For receiving the push 
		std::shared_ptr<pvd::Stream> CreateStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<std::shared_ptr<MediaTrack>> &tracks);
		std::shared_ptr<pvd::Stream> CreateStream(const ov::String &stream_name, const std::vector<std::shared_ptr<MediaTrack>> &tracks);
		// For pulling
		std::shared_ptr<pvd::Stream> CreateStream(const ov::String &stream_name, const std::vector<ov::String> &url_list);

		// Delete stream
		virtual bool DeleteStream(const std::shared_ptr<Stream> &stream);
		bool DeleteAllStreams();

		const char* GetApplicationTypeName() final;

	protected:
		explicit Application(const std::shared_ptr<Provider> &provider, const info::Application &application_info);
		~Application() override;

		// For child
		virtual std::shared_ptr<pvd::Stream> CreatePushStream(const uint32_t stream_id, const ov::String &stream_name) = 0;
		virtual std::shared_ptr<pvd::Stream> CreatePullStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<ov::String> &url_list) = 0;

	private:

		std::shared_ptr<StreamMotor> CreateStreamMotorInternal(const std::shared_ptr<Stream> &stream);
		bool DeleteStreamMotorInternal(const std::shared_ptr<Stream> &stream);
		bool DeleteStreamInternal(const std::shared_ptr<Stream> &stream);
		std::shared_ptr<StreamMotor> GetStreamMotorInternal(const std::shared_ptr<Stream> &stream);

		bool NotifyStreamCreated(std::shared_ptr<Stream> stream);
		bool NotifyStreamDeleted(std::shared_ptr<Stream> stream);

		uint32_t GetStreamMotorId(const std::shared_ptr<Stream> &stream);

		// Remove unused streams
		void WhiteElephantStreamCollector();
		
		bool _stop_collector_thread_flag;
		std::thread _collector_thread;

		std::shared_ptr<Provider>	_provider;
		
		std::shared_mutex 							_streams_guard;
		std::map<uint32_t, std::shared_ptr<Stream>> _streams;
		std::map<uint32_t, std::shared_ptr<StreamMotor>> _stream_motors;

		ApplicationState		_state = ApplicationState::Idle;
		std::atomic<info::stream_id_t>	_last_issued_stream_id { 0 };
	};
}
