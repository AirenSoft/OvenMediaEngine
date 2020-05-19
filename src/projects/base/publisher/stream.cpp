#include "stream.h"
#include "application.h"
#include "publisher_private.h"

namespace pub
{
	StreamWorker::StreamWorker(const std::shared_ptr<Stream> &parent_stream)
		: _packet_queue(nullptr, 100)
	{
		_stop_thread_flag = true;
		_parent = parent_stream;
	}

	StreamWorker::~StreamWorker()
	{
		Stop();
	}

	bool StreamWorker::Start()
	{
		if (!_stop_thread_flag)
		{
			return true;
		}

		ov::String queue_name;

		queue_name.Format("%s/%s/%s StreamWorker Queue", _parent->GetApplication()->GetApplicationTypeName(), _parent->GetApplication()->GetName().CStr(), _parent->GetName().CStr());
		_packet_queue.SetAlias(queue_name.CStr());
		
		_stop_thread_flag = false;
		_worker_thread = std::thread(&StreamWorker::WorkerThread, this);

		return true;
	}

	bool StreamWorker::Stop()
	{
		if (_stop_thread_flag)
		{
			return true;
		}

		_stop_thread_flag = true;
		// Generate Event
		_queue_event.Notify();
		if(_worker_thread.joinable())
		{
			_worker_thread.join();
		}

		std::lock_guard<std::shared_mutex> lock(_session_map_mutex);
		for (auto const &x : _sessions)
		{
			auto session = std::static_pointer_cast<Session>(x.second);
			session->Stop();
		}
		_sessions.clear();

		return true;
	}

	bool StreamWorker::AddSession(std::shared_ptr<Session> session)
	{
		std::lock_guard<std::shared_mutex> lock(_session_map_mutex);
		_sessions[session->GetId()] = session;

		return true;
	}

	bool StreamWorker::RemoveSession(session_id_t id)
	{
		// 해당 Session ID를 가진 StreamWorker를 찾아서 삭제한다.
		std::lock_guard<std::shared_mutex> lock(_session_map_mutex);
		if (_sessions.count(id) <= 0)
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
		std::shared_lock<std::shared_mutex> lock(_session_map_mutex);
		if (_sessions.count(id) <= 0)
		{
			logte("Cannot find session : %u", id);
			return nullptr;
		}

		return _sessions[id];
	}

	void StreamWorker::SendPacket(uint32_t type, std::shared_ptr<ov::Data> packet)
	{
		auto stream_packet = std::make_shared<pub::StreamWorker::StreamPacket>(type, packet);
		_packet_queue.Enqueue(std::move(stream_packet));

		_queue_event.Notify();
	}

	std::shared_ptr<StreamWorker::StreamPacket> StreamWorker::PopStreamPacket()
	{
		if (_packet_queue.IsEmpty())
		{
			return nullptr;
		}

		auto data = _packet_queue.Dequeue();
		if(data.has_value())
		{
			return data.value();
		}

		return nullptr;
	}

	void StreamWorker::WorkerThread()
	{
		std::shared_lock<std::shared_mutex> session_lock(_session_map_mutex, std::defer_lock);
		// Queue Event를 기다린다.
		while (!_stop_thread_flag)
		{
			// Queue에 이벤트가 들어올때까지 무한 대기 한다.
			// TODO: 향후 App 재시작 등의 기능을 위해 WaitFor(time) 기능을 구현한다.
			_queue_event.Wait();

			// Queue에서 패킷을 꺼낸다.
			std::shared_ptr<StreamWorker::StreamPacket> packet = PopStreamPacket();
			if (packet == nullptr)
			{
				continue;
			}

			session_lock.lock();
			// 모든 Session에 전송한다.
			for (auto const &x : _sessions)
			{
				auto session = std::static_pointer_cast<Session>(x.second);

				// Session will change data
				std::shared_ptr<ov::Data> session_data = packet->_data->Clone();
				session->SendOutgoingData(packet->_type, session_data);
			}
			session_lock.unlock();
		}
	}

	Stream::Stream(const std::shared_ptr<Application> application, const info::Stream &info)
		: info::Stream(info)
	{
		_application = application;
		_run_flag = false;
		_last_issued_session_id = 100;
	}

	Stream::~Stream()
	{
		Stop();
	}

	bool Stream::Start(uint32_t worker_count)
	{
		if (_run_flag == true)
		{
			return false;
		}

		if (worker_count > MAX_STREAM_WORKER_THREAD_COUNT)
		{
			worker_count = MAX_STREAM_WORKER_THREAD_COUNT;
		}

		_worker_count = worker_count;
		// Create WorkerThread
		for (uint32_t i = 0; i < _worker_count; i++)
		{
			auto stream_worker = std::make_shared<StreamWorker>(GetSharedPtr());
						
			if (stream_worker->Start() == false)
			{
				logte("Cannot create stream thread (%d)", i);
				Stop();

				return false;
			}

			_stream_workers[i] = stream_worker;
		}

		logti("%s application has started [%s(%u)] stream", _application->GetApplicationTypeName(), GetName().CStr(), GetId());

		_run_flag = true;
		return true;
	}

	bool Stream::Stop()
	{
		if (_run_flag == false)
		{
			return false;
		}

		_run_flag = false;

		for (uint32_t i = 0; i < _worker_count; i++)
		{
			_stream_workers[i]->Stop();
		}

		std::lock_guard<std::shared_mutex> session_lock(_session_map_mutex);
		for(const auto &x : _sessions)
		{
			auto session = x.second;
			session->Stop();
		}
		_sessions.clear();

		logti("[%s(%u)] %s stream has been stopped", GetName().CStr(), GetId(), _application->GetApplicationTypeName());

		return true;
	}

	std::shared_ptr<Application> Stream::GetApplication()
	{
		return _application;
	}

	std::shared_ptr<StreamWorker> Stream::GetWorkerByStreamID(session_id_t session_id)
	{
		return _stream_workers[session_id % _worker_count];
	}

	bool Stream::AddSession(std::shared_ptr<Session> session)
	{
		std::lock_guard<std::shared_mutex> session_lock(_session_map_mutex);
		// For getting session, all sessions
		_sessions[session->GetId()] = session;
		// 가장 적은 Session을 처리하는 Worker를 찾아서 Session을 넣는다.
		// session id로 hash를 만들어서 분배한다.
		return GetWorkerByStreamID(session->GetId())->AddSession(session);
	}

	bool Stream::RemoveSession(session_id_t id)
	{
		std::lock_guard<std::shared_mutex> session_lock(_session_map_mutex);
		if (_sessions.count(id) <= 0)
		{
			logte("Cannot find session : %u", id);
			return false;
		}

		_sessions.erase(id);

		return GetWorkerByStreamID(id)->RemoveSession(id);
	}

	std::shared_ptr<Session> Stream::GetSession(session_id_t id)
	{
		std::shared_lock<std::shared_mutex> session_lock(_session_map_mutex);
		return GetWorkerByStreamID(id)->GetSession(id);
	}

	const std::map<session_id_t, std::shared_ptr<Session>> Stream::GetAllSessions()
	{
		std::shared_lock<std::shared_mutex> session_lock(_session_map_mutex);
		return _sessions;
	}

	uint32_t Stream::GetSessionCount()
	{
		std::shared_lock<std::shared_mutex> session_lock(_session_map_mutex);
		return _sessions.size();
	}

	bool Stream::BroadcastPacket(uint32_t packet_type, std::shared_ptr<ov::Data> packet)
	{
		// 모든 StreamWorker에 나눠준다.
		for (uint32_t i = 0; i < _worker_count; i++)
		{
			_stream_workers[i]->SendPacket(packet_type, packet);
		}

		return true;
	}

	uint32_t Stream::IssueUniqueSessionId()
	{
		auto new_session_id = _last_issued_session_id++;

		while (true)
		{
			if (_sessions.find(new_session_id) == _sessions.end())
			{
				// not found
				break;
			}

			new_session_id++;
		}

		_last_issued_session_id = new_session_id;

		return new_session_id;
	}
}  // namespace pub