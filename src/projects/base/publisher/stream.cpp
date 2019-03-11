#include "publisher_private.h"
#include "stream.h"

StreamWorker::StreamWorker()
{
}

StreamWorker::~StreamWorker()
{
	Stop();
}

bool StreamWorker::Start()
{
	_stop_thread_flag = false;
	_worker_thread = std::thread(&StreamWorker::WorkerThread, this);

	return true;
}

bool StreamWorker::Stop()
{
	_stop_thread_flag = true;

	// Generate Event
	_queue_event.Notify();

	//_worker_thread.join();

	return true;
}

bool StreamWorker::AddSession(std::shared_ptr<Session> session)
{
	std::unique_lock<std::mutex> lock(_session_map_guard);
	_sessions[session->GetId()] = session;

	return true;
}

bool StreamWorker::RemoveSession(session_id_t id)
{
	// 해당 Session ID를 가진 StreamWorker를 찾아서 삭제한다.
	std::unique_lock<std::mutex> lock(_session_map_guard);
	if(_sessions.count(id) <= 0)
	{
		logtd("Cannot find session : %u", id);
		return false;
	}

	auto session = _sessions[id];
	// Session에 더이상 패킷을 전달하지 않는 것이 먼저다.
	_sessions.erase(id);

	// Session 동작을 중지한다.
	session->Stop();

	return true;
}

std::shared_ptr<Session> StreamWorker::GetSession(session_id_t id)
{
	if(_sessions.count(id) <= 0)
	{
		logtd("Cannot find session : %u", id);
		return nullptr;
	}

	return _sessions[id];
}

void StreamWorker::SendPacket(uint32_t id, std::shared_ptr<ov::Data> packet)
{
	// Queue에 패킷을 집어넣는다.
	auto stream_packet = std::make_shared<StreamWorker::StreamPacket>(id, packet);

	// Mutex (This function may be called by IcePort thread)
	std::unique_lock<std::mutex> lock(_packet_queue_guard);
	_packet_queue.push(std::move(stream_packet));
	lock.unlock();

	_queue_event.Notify();
}

std::shared_ptr<StreamWorker::StreamPacket> StreamWorker::PopStreamPacket()
{
	std::unique_lock<std::mutex> lock(_packet_queue_guard);

	if(_packet_queue.empty())
	{
		return nullptr;
	}

	// 데이터를 하나 꺼낸다.
	auto data = std::move(_packet_queue.front());
	_packet_queue.pop();

	return std::move(data);
}

void StreamWorker::WorkerThread()
{
	// Queue Event를 기다린다.
	while(!_stop_thread_flag)
	{
		// Queue에 이벤트가 들어올때까지 무한 대기 한다.
		// TODO: 향후 App 재시작 등의 기능을 위해 WaitFor(time) 기능을 구현한다.
		_queue_event.Wait();

		// Queue에서 패킷을 꺼낸다.
		std::shared_ptr<StreamWorker::StreamPacket> packet = PopStreamPacket();
		if(packet == nullptr)
		{
			continue;
		}

		// 모든 Session에 전송한다.
		for(auto const &x : _sessions)
		{
			auto session = std::static_pointer_cast<Session>(x.second);
			session->SendOutgoingData(packet->_id, packet->_data);
		}
	}
}

Stream::Stream(const std::shared_ptr<Application> application,
               const StreamInfo &info)
	: StreamInfo(info)
{
	_application = application;
	_run_flag = false;
}

Stream::~Stream()
{
	Stop();
}

bool Stream::Start(uint32_t worker_count)
{
	_worker_count = worker_count;
	// Create WorkerThread
	for(int i=0; i<_worker_count; i++)
	{
		if(!_stream_workers[i].Start())
		{
			logte("Cannot create stream thread (%d)", i);

			Stop();

			return false;
		}
	}

	_run_flag = true;
	return true;
}

bool Stream::Stop()
{
	_run_flag = false;

	for(int i=0; i<_worker_count; i++)
	{
		_stream_workers[i].Stop();
	}

	return true;
}

std::shared_ptr<Application> Stream::GetApplication()
{
	return _application;
}

StreamWorker& Stream::GetWorkerByStreamID(session_id_t session_id)
{
	logte("Select worker : %d", session_id % _worker_count);
	return _stream_workers[session_id % _worker_count];
}

bool Stream::AddSession(std::shared_ptr<Session> session)
{
	// For getting session, all sessions
	_sessions[session->GetId()] = session;
	// 가장 적은 Session을 처리하는 Worker를 찾아서 Session을 넣는다.
	// session id로 hash를 만들어서 분배한다.
	return GetWorkerByStreamID(session->GetId()).AddSession(session);
}

bool Stream::RemoveSession(session_id_t id)
{
	if(_sessions.count(id) <= 0)
	{
		logtd("Cannot find session : %u", id);
		return false;
	}
	_sessions.erase(id);

	return GetWorkerByStreamID(id).RemoveSession(id);
}

std::shared_ptr<Session> Stream::GetSession(session_id_t id)
{
	return GetWorkerByStreamID(id).GetSession(id);
}

const std::map<session_id_t, std::shared_ptr<Session>> &Stream::GetAllSessions()
{
	return _sessions;
}

bool Stream::BroadcastPacket(uint32_t packet_id, std::shared_ptr<ov::Data> packet)
{
	// 모든 StreamWorker에 나눠준다.
	for(int i=0; i<_worker_count; i++)
	{
		_stream_workers[i].SendPacket(packet_id, packet);
	}

	return true;
}