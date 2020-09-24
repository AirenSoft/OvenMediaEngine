#pragma once

#include <shared_mutex>
#include "base/common_types.h"
#include "base/info/stream.h"
#include "base/mediarouter/media_buffer.h"
#include "session.h"

#define MIN_STREAM_WORKER_THREAD_COUNT 2
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

		bool AddSession(std::shared_ptr<Session> session);
		bool RemoveSession(session_id_t id);
		std::shared_ptr<Session> GetSession(session_id_t id);

		void SendPacket(const std::any &packet);

	private:
		void WorkerThread();

		std::map<session_id_t, std::shared_ptr<Session>> _sessions;
		std::shared_mutex _session_map_mutex;
		ov::Semaphore _queue_event;

		std::any PopStreamPacket();
		ov::Queue<std::any> _packet_queue;

		bool _stop_thread_flag;
		std::thread _worker_thread;

		std::shared_ptr<Stream> _parent;
	};

	class Application;
	class Stream : public info::Stream, public ov::EnableSharedFromThis<Stream>
	{
	public:
		// Session을 추가한다.
		bool AddSession(std::shared_ptr<Session> session);
		bool RemoveSession(session_id_t id);
		std::shared_ptr<Session> GetSession(session_id_t id);
		const std::map<session_id_t, std::shared_ptr<Session>> GetAllSessions();
		uint32_t GetSessionCount();

		// A child call this function to delivery packet to all sessions
		bool BroadcastPacket(const std::any &packet);

		// Child must implement this function for packetizing and call BroadcastPacket to delivery to all sessions.
		virtual void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) = 0;
		virtual void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) = 0;

		virtual bool Start();
		virtual bool Stop();

		bool CreateStreamWorker(uint32_t worker_count);

		uint32_t IssueUniqueSessionId();

		std::shared_ptr<Application> GetApplication();
		const char * GetApplicationTypeName();

	protected:
		Stream(const std::shared_ptr<Application> application, const info::Stream &info);
		virtual ~Stream();
		
	private:
		std::shared_ptr<StreamWorker> GetWorkerByStreamID(session_id_t session_id);
		std::map<session_id_t, std::shared_ptr<Session>> _sessions;
		std::shared_mutex _session_map_mutex;

		uint32_t _worker_count;
		bool _run_flag;
		
		std::shared_mutex _stream_worker_lock;
		std::vector<std::shared_ptr<StreamWorker>>	_stream_workers;
		std::shared_ptr<Application> _application;

		session_id_t _last_issued_session_id;
	};
}  // namespace pub