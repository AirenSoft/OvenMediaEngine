#pragma once

#include <shared_mutex>
#include "base/common_types.h"
#include "base/info/stream.h"
#include "base/info/push.h"
#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_event.h"
#include "modules/managed_queue/managed_queue.h"
#include "session.h"

#define MAX_STREAM_WORKER_THREAD_COUNT 72

namespace pub
{
	class StreamWorker
	{
	public:
		StreamWorker(const std::shared_ptr<Stream> &parent_stream);
		~StreamWorker();

		bool Start();
		bool Stop();

		bool AddSession(const std::shared_ptr<Session> &session);
		bool RemoveSession(session_id_t id);
		std::shared_ptr<Session> GetSession(session_id_t id);

		// Send to a specific session
		void SendMessage(const std::shared_ptr<Session> &session, const std::any &message);

		// Send to all sessions
		void SendPacket(const std::any &packet);

	private:
		void WorkerThread();

		std::map<session_id_t, std::shared_ptr<Session>> _sessions;
		std::shared_mutex _session_map_mutex;
		
		ov::Semaphore _queue_event;

		std::optional<std::any> PopStreamPacket();
		ov::ManagedQueue<std::any> _packet_queue;

		struct SessionMessage
		{
			SessionMessage(const std::shared_ptr<Session> &session, const std::any &message)
			{
				_session = session;
				_message = message;
			}
			
			std::shared_ptr<Session> _session;
			std::any _message;
		};

		std::shared_ptr<SessionMessage> PopSessionMessage();
		ov::Queue<std::shared_ptr<SessionMessage>> _session_message_queue;

		std::atomic<bool> _stop_thread_flag;
		std::thread _worker_thread;

		std::shared_ptr<Stream> _parent;
	};

	class Application;
	class Stream : public info::Stream, public ov::EnableSharedFromThis<Stream>
	{
	public:

		// Create stream --> Start stream --> Stop stream --> Delete stream
		enum class State : uint8_t
		{
			CREATED = 0,
			STARTED,
			STOPPED,
			ERROR,
		};

		// Session을 추가한다.
		bool AddSession(std::shared_ptr<Session> session);
		bool RemoveSession(session_id_t id);
		std::shared_ptr<Session> GetSession(session_id_t id);
		const std::map<session_id_t, std::shared_ptr<Session>> GetAllSessions();
		uint32_t GetSessionCount();

		// This function is only called by Push Publisher
		virtual	std::shared_ptr<pub::Session> CreatePushSession(std::shared_ptr<info::Push> &push);

		// A child call this function to delivery packet to all sessions
		bool BroadcastPacket(const std::any &packet);

		bool SendMessage(const std::shared_ptr<Session> &session, const std::any &message);

		// Child must implement this function for packetizing and call BroadcastPacket to delivery to all sessions.
		virtual void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) = 0;
		virtual void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) = 0;
		virtual void SendDataFrame(const std::shared_ptr<MediaPacket> &media_packet) = 0;
		virtual void OnEvent(const std::shared_ptr<MediaEvent> &event) {}

		virtual bool Start();
		virtual bool Stop();
		virtual bool OnStreamUpdated(const std::shared_ptr<info::Stream> &info);

		bool WaitUntilStart(uint32_t timeout_ms);

		bool CreateStreamWorker(uint32_t worker_count);

		uint32_t IssueUniqueSessionId();

		std::shared_ptr<Application> GetApplication() const;
		const char * GetApplicationTypeName() const;

		// Set the stream state
		void SetState(State state)
		{
			_state = state;
		}

		State GetState() const
		{
			return _state;
		}

		const std::chrono::system_clock::time_point &GetStartedTime() const;

	protected:
		Stream(const std::shared_ptr<Application> application, const info::Stream &info);
		virtual ~Stream();

	private:
		std::shared_ptr<StreamWorker> GetWorkerBySessionID(session_id_t session_id);
		std::map<session_id_t, std::shared_ptr<Session>> _sessions;
		std::shared_mutex _session_map_mutex;

		uint32_t _worker_count;
		
		std::shared_mutex _stream_worker_lock;
		std::vector<std::shared_ptr<StreamWorker>>	_stream_workers;
		std::shared_ptr<Application> _application;

		session_id_t _last_issued_session_id;

		std::chrono::system_clock::time_point _started_time;

		State _state = State::CREATED;
	};
}  // namespace pub