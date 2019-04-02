#pragma once

#include "session.h"
#include "base/common_types.h"
#include "base/application/stream_info.h"
#include "application.h"

#define MIN_STREAM_THREAD_COUNT     2
#define MAX_STREAM_THREAD_COUNT     72

class StreamWorker
{
public:
	StreamWorker();
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
	std::mutex          _session_map_guard;
	ov::Semaphore       _queue_event;

	class StreamPacket
	{
	public:
		StreamPacket(uint32_t type, std::shared_ptr<ov::Data> data)
		{
			_type = type;
			_data = data->Clone();
		}

		uint32_t                    _type;
		std::shared_ptr<ov::Data>   _data;
	};

	std::shared_ptr<StreamPacket> PopStreamPacket();

	std::queue<std::shared_ptr<StreamPacket>>   _packet_queue;
	std::mutex      _packet_queue_guard;

	bool            _stop_thread_flag;
	std::thread     _worker_thread;

	std::shared_ptr<Stream> _parent;
};

class Stream : public StreamInfo, public ov::EnableSharedFromThis<Stream>
{
public:
	// Session을 추가한다.
	bool AddSession(std::shared_ptr<Session> session);
	bool RemoveSession(session_id_t id);
	std::shared_ptr<Session> GetSession(session_id_t id);
	const std::map<session_id_t, std::shared_ptr<Session>> &GetAllSessions();

	// Child call this function to delivery packet to all sessions
	bool BroadcastPacket(uint32_t packet_type, std::shared_ptr<ov::Data> packet);

	// Child must implement this function for packetizing and call BroadcastPacket to delivery to all sessions.
	virtual void SendVideoFrame(std::shared_ptr<MediaTrack> track,
	                            std::unique_ptr<EncodedFrame> encoded_frame,
	                            std::unique_ptr<CodecSpecificInfo> codec_info,
	                            std::unique_ptr<FragmentationHeader> fragmentation) = 0;

	virtual void SendAudioFrame(std::shared_ptr<MediaTrack> track,
	                            std::unique_ptr<EncodedFrame> encoded_frame,
	                            std::unique_ptr<CodecSpecificInfo> codec_info,
	                            std::unique_ptr<FragmentationHeader> fragmentation) = 0;

	virtual bool Start(uint32_t worker_count);
	virtual bool Stop();
protected:
	Stream(const std::shared_ptr<Application> application, const StreamInfo &info);
	virtual ~Stream();

	std::shared_ptr<Application>    GetApplication();

private:
	StreamWorker&                   GetWorkerByStreamID(session_id_t session_id);
	std::map<session_id_t, std::shared_ptr<Session>> _sessions;

	uint32_t                        _worker_count;
	bool                            _run_flag;
	StreamWorker                    _stream_workers[MAX_STREAM_THREAD_COUNT];
	std::shared_ptr<Application>    _application;
};