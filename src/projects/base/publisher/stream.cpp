#include "stream.h"
#include "application.h"
#include "publisher_private.h"

namespace pub
{
	StreamWorker::StreamWorker(const std::shared_ptr<Stream> &parent_stream)
		: _packet_queue(nullptr, 500)
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

		queue_name.Format("%s/%s/%s StreamWorker Queue", _parent->GetApplicationTypeName(), _parent->GetApplicationName(), _parent->GetName().CStr());
		_packet_queue.SetAlias(queue_name.CStr());
		
		_stop_thread_flag = false;
		_worker_thread = std::thread(&StreamWorker::WorkerThread, this);
		pthread_setname_np(_worker_thread.native_handle(), "StreamWorker");

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
		_packet_queue.Stop();
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
		// Cannot add session after StreamWorker is stopped
		if (_stop_thread_flag)
		{
			return true;
		}

		std::lock_guard<std::shared_mutex> lock(_session_map_mutex);
		_sessions[session->GetId()] = session;

		return true;
	}

	bool StreamWorker::RemoveSession(session_id_t id)
	{
		// Cannot remove session after StreamWorker is stopped
		if (_stop_thread_flag)
		{
			return true;
		}

		std::unique_lock<std::shared_mutex> lock(_session_map_mutex);
		if (_sessions.count(id) <= 0)
		{
			logte("Cannot find session : %u", id);
			return false;
		}

		auto session = _sessions[id];
		_sessions.erase(id);
		lock.unlock();

		session->Stop();

		return true;
	}

	std::shared_ptr<Session> StreamWorker::GetSession(session_id_t id)
	{
		std::shared_lock<std::shared_mutex> lock(_session_map_mutex);
		if (_sessions.count(id) <= 0)
		{
			// logte("Cannot find session : %u", id);
			return nullptr;
		}

		return _sessions[id];
	}

	void StreamWorker::SendPacket(const std::any &packet)
	{
		_packet_queue.Enqueue(packet);
		_queue_event.Notify();
	}

	std::any StreamWorker::PopStreamPacket()
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

		while (!_stop_thread_flag)
		{
			_queue_event.Wait();

			auto packet = PopStreamPacket();
			if (!packet.has_value())
			{
				continue;
			}
			
			session_lock.lock();
			for (auto const &x : _sessions)
			{
				auto session = std::static_pointer_cast<Session>(x.second);
				session->SendOutgoingData(packet);
			}
			session_lock.unlock();
		}
	}

	Stream::Stream(const std::shared_ptr<Application> application, const info::Stream &info)
		: info::Stream(info)
	{
		_application = application;
		_last_issued_session_id = 100;
		_state = State::CREATED;
	}

	Stream::~Stream()
	{
		Stop();
	}

	bool Stream::Start()
	{
		if (_state != State::CREATED)
		{
			return false;
		}

		logti("%s application has started [%s(%u)] stream", GetApplicationTypeName(), GetName().CStr(), GetId());
		_state = State::STARTED;
		return true;
	}

	bool Stream::WaitUntilStart(uint32_t timeout_ms)
	{
		ov::StopWatch	watch;
		
		watch.Start();

		while(_state != State::STARTED && watch.Elapsed() < timeout_ms)
		{
			usleep(100 * 1000); // 100ms
		}

		return _state == State::STARTED;
	}

	bool Stream::CreateStreamWorker(uint32_t worker_count)
	{
		std::unique_lock<std::shared_mutex> worker_lock(_stream_worker_lock);
		
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

			_stream_workers.push_back(stream_worker);
		}

		worker_lock.unlock();

		return true;
	}

	bool Stream::Stop()
	{
		std::unique_lock<std::shared_mutex> worker_lock(_stream_worker_lock);

		if (_state != State::STARTED)
		{
			return false;
		}

		_state = State::STOPPED;

		for(const auto &worker : _stream_workers)
		{
			worker->Stop();
		}

		_stream_workers.clear();

		worker_lock.unlock();

		std::lock_guard<std::shared_mutex> session_lock(_session_map_mutex);
		for(const auto &x : _sessions)
		{
			auto session = x.second;
			session->Stop();
		}
		_sessions.clear();

		logti("[%s(%u)] %s stream has been stopped", GetName().CStr(), GetId(), GetApplicationTypeName());

		return true;
	}

	std::shared_ptr<Application> Stream::GetApplication()
	{
		return _application;
	}

	const char * Stream::GetApplicationTypeName()
	{
		if(GetApplication() == nullptr)
		{
			return "Unknown";
		}

		return GetApplication()->GetApplicationTypeName();
	}

	std::shared_ptr<StreamWorker> Stream::GetWorkerBySessionID(session_id_t session_id)
	{
		if(_worker_count == 0)
		{
			return nullptr;
		}
		std::shared_lock<std::shared_mutex> worker_lock(_stream_worker_lock);
		return _stream_workers[session_id % _worker_count];
	}

	bool Stream::AddSession(std::shared_ptr<Session> session)
	{
		std::lock_guard<std::shared_mutex> session_lock(_session_map_mutex);
		// For getting session, all sessions
		_sessions[session->GetId()] = session;

		if(_worker_count > 0)
		{
			return GetWorkerBySessionID(session->GetId())->AddSession(session);
		}

		return true;
	}

	bool Stream::RemoveSession(session_id_t id)
	{
		std::unique_lock<std::shared_mutex> session_lock(_session_map_mutex);
		if (_sessions.count(id) <= 0)
		{
			logte("Cannot find session : %u", id);
			return false;
		}
		_sessions.erase(id);
		session_lock.unlock();

		if(_worker_count > 0)
		{
			return GetWorkerBySessionID(id)->RemoveSession(id);
		}

		return true;
	}

	std::shared_ptr<Session> Stream::GetSession(session_id_t id)
	{
		std::shared_lock<std::shared_mutex> session_lock(_session_map_mutex);
		if (_sessions.count(id) <= 0)
		{
			return nullptr;
		}

		return _sessions[id];
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

	bool Stream::BroadcastPacket(const std::any &packet)
	{
		if(_worker_count > 0)
		{
			std::shared_lock<std::shared_mutex> worker_lock(_stream_worker_lock);
			for (uint32_t i = 0; i < _stream_workers.size(); i++)
			{
				_stream_workers[i]->SendPacket(packet);
			}
		}
		else
		{
			std::shared_lock<std::shared_mutex> session_lock(_session_map_mutex);
			for (auto const &x : _sessions)
			{
				auto session = std::static_pointer_cast<Session>(x.second);
				session->SendOutgoingData(packet);
			}
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