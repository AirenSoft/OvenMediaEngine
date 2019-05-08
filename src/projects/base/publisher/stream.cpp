#include "publisher_private.h"
#include "stream.h"

StreamWorker::StreamWorker()
{
	_stop_thread_flag = true;
}

StreamWorker::~StreamWorker()
{
	Stop();
}

bool StreamWorker::Start()
{
	if(!_stop_thread_flag)
	{
		return true;
	}
	_stop_thread_flag = false;
	_worker_thread = std::thread(&StreamWorker::WorkerThread, this);

	return true;
}

bool StreamWorker::Stop()
{
	if(_stop_thread_flag)
	{
		return true;
	}

	_stop_thread_flag = true;
	// Generate Event
	_queue_event.Notify();
	_worker_thread.join();

	for(auto const &x : _sessions)
	{
		auto session = std::static_pointer_cast<Session>(x.second);
		session->Stop();
	}
	_sessions.clear();

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
		logte("Cannot find session : %u", id);
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
        logte("Cannot find session : %u", id);
		return nullptr;
	}

	return _sessions[id];
}

void StreamWorker::SendPacket(uint32_t type, std::shared_ptr<ov::Data> packet)
{
	// Queue에 패킷을 집어넣는다.
	auto stream_packet = std::make_shared<StreamWorker::StreamPacket>(type, packet);

	std::unique_lock<std::mutex> lock(_packet_queue_guard);
	_packet_queue.push(stream_packet);
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

	auto data = _packet_queue.front();
	_packet_queue.pop();

	return std::move(data);
}

void StreamWorker::WorkerThread()
{
	std::unique_lock<std::mutex> session_lock(_session_map_guard, std::defer_lock);
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

		session_lock.lock();
		// 모든 Session에 전송한다.
		for(auto const &x : _sessions)
		{
			auto session = std::static_pointer_cast<Session>(x.second);

			// Session will change data
			std::shared_ptr<ov::Data> session_data = packet->_data->Clone();
			session->SendOutgoingData(packet->_type, session_data);
		}
		session_lock.unlock();
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
	if(_run_flag == true)
	{
		return false;
	}

	if(worker_count > MAX_STREAM_THREAD_COUNT)
	{
		worker_count = MAX_STREAM_THREAD_COUNT;
	}

	_worker_count = worker_count;
	// Create WorkerThread
	for(uint32_t i=0; i<_worker_count; i++)
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
	if(_run_flag == false)
	{
		return false;
	}

	_run_flag = false;

	for(uint32_t i=0; i<_worker_count; i++)
	{
		_stream_workers[i].Stop();
	}

	_sessions.clear();

	return true;
}

std::shared_ptr<Application> Stream::GetApplication()
{
	return _application;
}

StreamWorker& Stream::GetWorkerByStreamID(session_id_t session_id)
{
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
		logte("Cannot find session : %u", id);
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

bool Stream::BroadcastPacket(uint32_t packet_type, std::shared_ptr<ov::Data> packet)
{
	// 모든 StreamWorker에 나눠준다.
	for(uint32_t i=0; i<_worker_count; i++)
	{
		_stream_workers[i].SendPacket(packet_type, packet);
	}

	return true;
}