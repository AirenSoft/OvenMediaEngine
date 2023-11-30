#pragma once

#include <utility>
#include <shared_mutex>
#include "base/common_types.h"
#include "base/info/stream.h"
#include "base/info/session.h"
#include "base/info/push.h"
#include "base/mediarouter/mediarouter_application_observer.h"
#include "base/ovlibrary/semaphore.h"
#include "base/ovlibrary/string.h"
#include "config/config.h"
#include "stream.h"
#include "modules/managed_queue/managed_queue.h"

#define MIN_APPLICATION_WORKER_COUNT		1
#define MAX_APPLICATION_WORKER_COUNT		72

namespace pub
{
	enum ApplicationState
	{
		Idle,
		Started,
		Stopped,
		Error
	};

	class Publisher;

	// Distribute Stream to applicationWorker.
	class ApplicationWorker
	{
	public:
		ApplicationWorker(uint32_t worker_id, ov::String vhost_app_name, ov::String worker_name);
		bool Start();
		bool Stop();
		bool PushMediaPacket(const std::shared_ptr<Stream> &stream, const std::shared_ptr<MediaPacket> &media_packet);

	private:
		void WorkerThread();

		uint32_t	_worker_id = 0;
		ov::String  _vhost_app_name;
		ov::String	_worker_name;

		class StreamData
		{
		public:
			StreamData(const std::shared_ptr<Stream> &stream,
							const std::shared_ptr<MediaPacket> &media_packet)
			{
				_stream = stream;
				_media_packet = media_packet;
			}

			std::shared_ptr<Stream> _stream;
			std::shared_ptr<MediaPacket> _media_packet;
		};
		std::shared_ptr<ApplicationWorker::StreamData> PopStreamData();

		std::atomic<bool> _stop_thread_flag;
		std::thread _worker_thread;
		ov::Semaphore _queue_event;

		ov::ManagedQueue<std::shared_ptr<StreamData>> _stream_data_queue;

		int64_t	_last_video_ts_ms = 0;
		int64_t	_last_audio_ts_ms = 0;
	};

	class Application : public info::Application, public MediaRouterApplicationObserver
	{
	public:
		const char* GetApplicationTypeName() final;
		const char* GetPublisherTypeName() final;
		
		// MediaRouterApplicationObserver Implementation
		bool OnStreamCreated(const std::shared_ptr<info::Stream> &info) override;
		bool OnStreamDeleted(const std::shared_ptr<info::Stream> &info) override;
		bool OnStreamPrepared(const std::shared_ptr<info::Stream> &info) override;
		bool OnStreamUpdated(const std::shared_ptr<info::Stream> &info) override;

		// Put data in ApplicationWorker's queue.
		bool OnSendFrame(const std::shared_ptr<info::Stream> &stream,
							  const std::shared_ptr<MediaPacket> &media_packet) override;

		uint32_t GetStreamCount();
		std::shared_ptr<Stream> GetStream(uint32_t stream_id);
		std::shared_ptr<Stream> GetStream(ov::String stream_name);

		virtual bool Start();
		virtual bool Stop();

	protected:
		explicit Application(const std::shared_ptr<Publisher> &publisher, const info::Application &application_info);
		virtual ~Application();

		std::shared_mutex 		_stream_map_mutex;
		std::map<uint32_t, std::shared_ptr<Stream>> _streams;

	private:
		bool DeleteAllStreams();
		virtual std::shared_ptr<Stream> CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t thread_count) = 0;
		virtual bool DeleteStream(const std::shared_ptr<info::Stream> &info) = 0;
		
		std::shared_ptr<ApplicationWorker> GetWorkerByStreamID(info::stream_id_t stream_id);

		uint32_t		_application_worker_count;
		std::shared_mutex _application_worker_lock;
		std::vector<std::shared_ptr<ApplicationWorker>>	_application_workers;

		std::shared_ptr<Publisher>		_publisher;
	};


	#define PUSH_PUBLISHER_ERROR_DOMAIN "PublisherPushApplication"

	class PushApplication : public Application
	{
	public:
		enum ErrorCode
		{
			Success,
			FailureInvalidParameter,
			FailureDuplicateKey,
			FailureNotExist,
			FailureUnknown
		};

		explicit PushApplication(const std::shared_ptr<Publisher> &publisher, const info::Application &application_info);

		virtual bool Start();
		virtual bool Stop();

		virtual std::shared_ptr<ov::Error> StartPush(const std::shared_ptr<info::Push> &push);
		virtual std::shared_ptr<ov::Error> StopPush(const std::shared_ptr<info::Push> &push);
		virtual std::shared_ptr<ov::Error> GetPushes(const std::shared_ptr<info::Push> push, std::vector<std::shared_ptr<info::Push>> &results);

		std::shared_ptr<info::Push> GetPushInfoById(ov::String id);
		void StartPushInternal(const std::shared_ptr<info::Push> &push, std::shared_ptr<pub::Session> session);
		void StopPushInternal(const std::shared_ptr<info::Push> &push, std::shared_ptr<pub::Session> session);
		void SessionControlThread();

		std::atomic<bool> _session_control_stop_thread_flag = false;
		std::thread _session_contol_thread;

		// <uniqueId, pushInfo>
		std::map<ov::String, std::shared_ptr<info::Push>> _pushes;
		std::shared_mutex _push_map_mutex;
	};
}  // namespace pub
