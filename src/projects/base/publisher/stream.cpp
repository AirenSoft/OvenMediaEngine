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
	}

	bool StreamWorker::Start()
	{
		if (!_stop_thread_flag)
		{
			return true;
		}

		auto urn = std::make_shared<info::ManagedQueue::URN>(
			_parent->GetApplicationName(), 
			_parent->GetName(), 
			"pub", 
			ov::String::FormatString("streamworker_%s", _parent->GetApplication()->GetPublisherTypeName()).LowerCaseString());
		_packet_queue.SetUrn(urn);
		
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

		ov::String worker_name = ov::String::FormatString("%s/%s/%s", _parent->GetApplicationTypeName(), _parent->GetApplicationName(), _parent->GetName().CStr());
		logtd("Try to stop StreamWorker thread of %s", worker_name.CStr());

		_stop_thread_flag = true;
		// Generate Event
		_packet_queue.Stop();
		_session_message_queue.Stop();
		_queue_event.Stop();
		
		if(_worker_thread.joinable())
		{
			_worker_thread.join();
		}

		logtd("StreamWorker thread of %s has been stopped successfully", worker_name.CStr());

		std::lock_guard<std::shared_mutex> lock(_session_map_mutex);

		logtd("Try to stop all sessions of %s", worker_name.CStr());
		for (auto const &x : _sessions)
		{
			auto session = std::static_pointer_cast<Session>(x.second);
			session->Stop();
		}
		_sessions.clear();
		logtd("All sessions(%d) of %s has been stopped successfully", _sessions.size(), worker_name.CStr());

		return true;
	}

	bool StreamWorker::AddSession(const std::shared_ptr<Session> &session)
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

	// Send to a specific session
	void StreamWorker::SendMessage(const std::shared_ptr<Session> &session, const std::any &message)
	{
		_session_message_queue.Enqueue(std::make_shared<SessionMessage>(session, message));
		_queue_event.Notify();
	}

	std::optional<std::any> StreamWorker::PopStreamPacket()
	{
		if (_packet_queue.IsEmpty())
		{
			return std::nullopt;
		}

		return _packet_queue.Dequeue();
	}

	std::shared_ptr<StreamWorker::SessionMessage> StreamWorker::PopSessionMessage()
	{
		if (_session_message_queue.IsEmpty())
		{
			return nullptr;
		}

		auto data = _session_message_queue.Dequeue();
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

			auto session_message = PopSessionMessage();
			if (session_message != nullptr && session_message->_session != nullptr && session_message->_message.has_value())
			{
				session_message->_session->OnMessageReceived(session_message->_message);
			}

			auto packet = PopStreamPacket();
			if (packet.has_value())
			{		
				session_lock.lock();
				for (auto const &x : _sessions)
				{
					auto session = x.second;
					session->SendOutgoingData(packet.value());
				}
				session_lock.unlock();
			}
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
	}

	std::shared_ptr<const info::Playlist> Stream::GetDefaultPlaylist() const
	{
		auto info = GetDefaultPlaylistInfo();

		if (info != nullptr)
		{
			return GetPlaylist(info->file_name);
		}

		return nullptr;
	}

	bool Stream::Start()
	{
		if (_state != State::CREATED)
		{
			return false;
		}

		logti("%s has started [%s(%u)] stream (MSID : %d)", GetApplicationTypeName(), GetName().CStr(), GetId(), GetMsid());

		_started_time = std::chrono::system_clock::now();
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
		logti("Try to stop %s stream [%s(%u)]", GetApplicationTypeName(), GetName().CStr(), GetId());

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

		logti("[%s(%u)] %s - All StreamWorker has been stopped", GetName().CStr(), GetId(), GetApplicationTypeName());

		_stream_workers.clear();

		worker_lock.unlock();

		std::lock_guard<std::shared_mutex> session_lock(_session_map_mutex);

		logti("[%s(%u)] %s - Try to stop all sessions (%d)", GetName().CStr(), GetId(), GetApplicationTypeName(), _sessions.size());

		for(const auto &x : _sessions)
		{
			auto session = x.second;
			session->Stop();
		}
		_sessions.clear();

		logti("[%s(%u)] %s stream has been stopped", GetName().CStr(), GetId(), GetApplicationTypeName());

		return true;
	}

	bool Stream::OnStreamUpdated(const std::shared_ptr<info::Stream> &info)
	{
		logti("[%s(%u)] %s stream has been updated (MSID : %d)", 
							info->GetName().CStr(), info->GetId(), GetApplicationTypeName(), info->GetMsid());

		return true;
	}

	std::shared_ptr<pub::Session> Stream::CreatePushSession(std::shared_ptr<info::Push> &push)
	{
		logtw("The function was not implemented in the child class");
		return nullptr;
	}

	const std::chrono::system_clock::time_point &Stream::GetStartedTime() const
	{
		return _started_time;
	}

	std::shared_ptr<Application> Stream::GetApplication() const
	{
		return _application;
	}

	const char * Stream::GetApplicationTypeName() const
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

		size_t worker_id = session_id % _worker_count;
		if(worker_id >= _stream_workers.size())
		{
			logtw("Invalid worker id : %d", worker_id);
			return nullptr;
		}

		return _stream_workers[worker_id];
	}

	bool Stream::AddSession(std::shared_ptr<Session> session)
	{
		std::lock_guard<std::shared_mutex> session_lock(_session_map_mutex);
		// For getting session, all sessions
		_sessions[session->GetId()] = session;

		if(_worker_count > 0)
		{
			auto worker = GetWorkerBySessionID(session->GetId());
			if(worker == nullptr)
			{
				logte("Cannot find worker for session : %u", session->GetId());
				return false;
			}

			return worker->AddSession(session);
		}

		return true;
	}

	bool Stream::RemoveSession(session_id_t id)
	{
		std::unique_lock<std::shared_mutex> session_lock(_session_map_mutex);
		if (_sessions.count(id) <= 0)
		{
			logtd("Cannot find session : %u", id);
			return false;
		}
		_sessions.erase(id);

		session_lock.unlock();

		if(_worker_count > 0)
		{
			auto worker = GetWorkerBySessionID(id);
			if (worker == nullptr)
			{
				logte("Cannot find worker for session : %u", id);
				return false;
			}

			return worker->RemoveSession(id);
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

	bool Stream::SendMessage(const std::shared_ptr<Session> &session, const std::any &message)
	{
		if(_worker_count > 0)
		{
			auto worker = GetWorkerBySessionID(session->GetId());
			if(worker == nullptr)
			{
				logtw("Cannot find worker for session : %u", session->GetId());
				return false;
			}
			worker->SendMessage(session, message);
		}
		else
		{
			session->OnMessageReceived(message);
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