#pragma once

#include <shared_mutex>
#include "base/common_types.h"
#include "base/info/stream.h"
#include "base/media_route/media_buffer.h"
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

		void SendPacket(uint32_t type, std::shared_ptr<ov::Data> packet);

	private:
		void WorkerThread();

		std::map<session_id_t, std::shared_ptr<Session>> _sessions;
		std::shared_mutex _session_map_mutex;
		ov::Semaphore _queue_event;

		class StreamPacket
		{
		public:
			StreamPacket(uint32_t type, std::shared_ptr<ov::Data> data)
			{
				_type = type;
				_data = data->Clone();
			}

			uint32_t _type;
			std::shared_ptr<ov::Data> _data;
		};

		std::shared_ptr<StreamPacket> PopStreamPacket();

		ov::Queue<std::shared_ptr<StreamPacket>> _packet_queue;

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
		bool BroadcastPacket(uint32_t packet_type, std::shared_ptr<ov::Data> packet);

		// Child must implement this function for packetizing and call BroadcastPacket to delivery to all sessions.
		virtual void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) = 0;
		virtual void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) = 0;

		virtual bool Start(uint32_t worker_count);
		virtual bool Stop();

		uint32_t IssueUniqueSessionId();

		std::shared_ptr<Application> GetApplication();

	protected:
		Stream(const std::shared_ptr<Application> application, const info::Stream &info);
		virtual ~Stream();
		
	private:
		std::shared_ptr<StreamWorker> GetWorkerByStreamID(session_id_t session_id);
		std::map<session_id_t, std::shared_ptr<Session>> _sessions;
		std::shared_mutex _session_map_mutex;

		uint32_t _worker_count;
		bool _run_flag;
		
		std::map<uint32_t, std::shared_ptr<StreamWorker>>	_stream_workers;
		std::shared_ptr<Application> _application;

		session_id_t _last_issued_session_id;
	};
}  // namespace pub