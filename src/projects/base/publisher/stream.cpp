#include "publisher_private.h"
#include "stream.h"

StreamWorker::StreamWorker()
{
}

bool StreamWorker::StartWorker(std::shared_ptr<Stream> parent)
{
	_stop_thread_flag = false;
	_worker_thread = std::thread(&StreamWorker::WorkerThread, this);

	return true;
}

bool StreamWorker::Stop()
{


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

	logtd("Session deleted : %u", id);

	return true;
}

void StreamWorker::WorkerThread()
{
	// Queue Event를 기다린다.

	// Queue에서 패킷을 꺼낸다.

	// 모든 Session에 전송한다.
	for(auto const &x : _sessions)
	{
		auto session = std::static_pointer_cast<Session>(x.second);

		// 2019.03.09 여기부터 작업하면 됨
		// session->SendOutgoingData(id, packet);
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
	_run_flag = false;
}

bool Stream::Start(uint32_t worker_count)
{
	_worker_count = worker_count;
	// Create WorkerThread
	for(int i=0; i<_worker_count; i++)
	{
		if(!_stream_workers[i].Start(this->GetSharedPtr()))
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

bool Stream::AddSession(std::shared_ptr<Session> session)
{
	// 가장 적은 Session을 처리하는 Worker를 찾아서 Session을 넣는다.
	// session id로 hash를 만들어서 분배한다.
	return _stream_workers[session->GetId() % _worker_count].AddSession(session);
}

bool Stream::RemoveSession(session_id_t id)
{
	return _stream_workers[id % _worker_count].RemoveSession(id);
}

bool Stream::BroadcastPacket(uint32_t packet_id, std::shared_ptr<ov::Data> packet)
{
	// 모든 StreamWorker에 나눠준다.
	for(int i=0; i<_worker_count; i++)
	{
		_stream_workers->SendPacket(packet_id, packet);
	}

	return true;
}