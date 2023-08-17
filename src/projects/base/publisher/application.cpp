#include "application.h"

#include <algorithm>

#include "publisher.h"
#include "publisher_private.h"

namespace pub
{
	ApplicationWorker::ApplicationWorker(uint32_t worker_id, ov::String vhost_app_name, ov::String worker_name)
		: _stream_data_queue(nullptr, 500)
	{
		_worker_id = worker_id;
		_vhost_app_name = vhost_app_name;
		_worker_name = worker_name;
		_stop_thread_flag = false;
	}

	bool ApplicationWorker::Start()
	{
		_stop_thread_flag = false;
		_worker_thread = std::thread(&ApplicationWorker::WorkerThread, this);

		auto name = ov::String::FormatString("AW-%s%d", _worker_name.CStr(), _worker_id);
		pthread_setname_np(_worker_thread.native_handle(), name.CStr());

		auto urn = std::make_shared<info::ManagedQueue::URN>(
			_vhost_app_name,
			nullptr,
			"pub",
			name.LowerCaseString());

		_stream_data_queue.SetUrn(urn);

		logtd("%s ApplicationWorker has been created", _worker_name.CStr());

		return true;
	}

	bool ApplicationWorker::Stop()
	{
		if (_stop_thread_flag == true)
		{
			return true;
		}

		_stream_data_queue.Clear();

		_stop_thread_flag = true;

		_queue_event.Stop();

		if (_worker_thread.joinable())
		{
			_worker_thread.join();
		}

		return true;
	}

	bool ApplicationWorker::PushMediaPacket(const std::shared_ptr<Stream> &stream, const std::shared_ptr<MediaPacket> &media_packet)
	{
		auto data = std::make_shared<ApplicationWorker::StreamData>(stream, media_packet);
		_stream_data_queue.Enqueue(std::move(data));

		_queue_event.Notify();

		return true;
	}

	std::shared_ptr<ApplicationWorker::StreamData> ApplicationWorker::PopStreamData()
	{
		if (_stream_data_queue.IsEmpty())
		{
			return nullptr;
		}

		auto data = _stream_data_queue.Dequeue(0);
		if (data.has_value())
		{
			return data.value();
		}

		return nullptr;
	}

	void ApplicationWorker::WorkerThread()
	{
		while (!_stop_thread_flag)
		{
			_queue_event.Wait();

			// Check media data is available
			auto stream_data = PopStreamData();
			if ((stream_data != nullptr) && (stream_data->_stream != nullptr) && (stream_data->_media_packet != nullptr))
			{
				if (stream_data->_media_packet->GetMediaType() == cmn::MediaType::Video)
				{
					stream_data->_stream->SendVideoFrame(stream_data->_media_packet);
				}
				else if (stream_data->_media_packet->GetMediaType() == cmn::MediaType::Audio)
				{
					stream_data->_stream->SendAudioFrame(stream_data->_media_packet);
				}
				else if (stream_data->_media_packet->GetMediaType() == cmn::MediaType::Data)
				{
					stream_data->_stream->SendDataFrame(stream_data->_media_packet);
				}
				else
				{
					// Nothing can do
				}
			}
		}
	}

	Application::Application(const std::shared_ptr<Publisher> &publisher, const info::Application &application_info)
		: info::Application(application_info)
	{
		_publisher = publisher;
	}

	Application::~Application()
	{
		Stop();
	}

	const char *Application::GetApplicationTypeName()
	{
		if (_publisher == nullptr)
		{
			return "";
		}

		if (_app_type_name.IsEmpty())
		{
			_app_type_name.Format("%s %s", _publisher->GetPublisherName(), "Application");
		}

		return _app_type_name.CStr();
	}

	const char *Application::GetPublisherTypeName()
	{
		if (_publisher_type_name.IsEmpty())
		{
			_publisher_type_name = StringFromPublisherType(_publisher->GetPublisherType());
		}

		return _publisher_type_name.CStr();
	}

	bool Application::Start()
	{
		_application_worker_count = GetConfig().GetAppWorkerCount();
		if (_application_worker_count < MIN_APPLICATION_WORKER_COUNT)
		{
			_application_worker_count = MIN_APPLICATION_WORKER_COUNT;
		}
		if (_application_worker_count > MAX_APPLICATION_WORKER_COUNT)
		{
			_application_worker_count = MAX_APPLICATION_WORKER_COUNT;
		}

		std::lock_guard<std::shared_mutex> worker_lock(_application_worker_lock);

		for (uint32_t i = 0; i < _application_worker_count; i++)
		{
			auto app_worker = std::make_shared<ApplicationWorker>(i, GetName().CStr(), StringFromPublisherType(_publisher->GetPublisherType()));
			if (app_worker->Start() == false)
			{
				logte("Cannot create ApplicationWorker (%s/%s/%d)", GetApplicationTypeName(), GetName().CStr(), i);
				Stop();

				return false;
			}

			_application_workers.push_back(app_worker);
		}

		logti("%s has created [%s] application", GetApplicationTypeName(), GetName().CStr());

		return true;
	}

	bool Application::Stop()
	{
		std::unique_lock<std::shared_mutex> worker_lock(_application_worker_lock);
		for (const auto &worker : _application_workers)
		{
			worker->Stop();
		}

		_application_workers.clear();
		worker_lock.unlock();

		// release remaining streams
		DeleteAllStreams();

		logti("%s has deleted [%s] application", GetApplicationTypeName(), GetName().CStr());

		return true;
	}

	bool Application::DeleteAllStreams()
	{
		std::unique_lock<std::shared_mutex> lock(_stream_map_mutex);

		for (const auto &x : _streams)
		{
			auto stream = x.second;
			stream->Stop();
		}

		_streams.clear();

		return true;
	}

	// Called by MediaRouteApplicationObserver
	bool Application::OnStreamCreated(const std::shared_ptr<info::Stream> &info)
	{
		auto stream_worker_count = GetConfig().GetStreamWorkerCount();

		auto stream = CreateStream(info, stream_worker_count);
		if (!stream)
		{
			return false;
		}

		std::lock_guard<std::shared_mutex> lock(_stream_map_mutex);
		_streams[info->GetId()] = stream;

		return true;
	}

	bool Application::OnStreamDeleted(const std::shared_ptr<info::Stream> &info)
	{
		std::unique_lock<std::shared_mutex> lock(_stream_map_mutex);

		auto stream_it = _streams.find(info->GetId());
		if (stream_it == _streams.end())
		{
			// Sometimes stream rejects stream creation if the input codec is not supported. So this is a normal situation.
			logtd("OnStreamDeleted failed. Cannot find stream : %s/%u", info->GetName().CStr(), info->GetId());
			return true;
		}

		auto stream = stream_it->second;

		lock.unlock();

		if (DeleteStream(info) == false)
		{
			return false;
		}

		lock.lock();
		_streams.erase(info->GetId());

		// Stop stream
		stream->Stop();

		return true;
	}

	bool Application::OnStreamPrepared(const std::shared_ptr<info::Stream> &info)
	{
		std::shared_lock<std::shared_mutex> lock(_stream_map_mutex);

		auto stream_it = _streams.find(info->GetId());
		if (stream_it == _streams.end())
		{
			// Sometimes stream rejects stream creation if the input codec is not supported. So this is a normal situation.
			logtd("OnStreamPrepared failed. Cannot find stream : %s/%u", info->GetName().CStr(), info->GetId());
			return true;
		}

		auto stream = stream_it->second;

		lock.unlock();

		// Start stream
		if (stream->Start() == false)
		{
			stream->SetState(Stream::State::ERROR);
			logtw("%s could not start [%s] stream.", GetApplicationTypeName(), info->GetName().CStr(), info->GetId());
			return false;
		}

		return true;
	}

	bool Application::OnStreamUpdated(const std::shared_ptr<info::Stream> &info)
	{
		std::shared_lock<std::shared_mutex> lock(_stream_map_mutex);

		auto stream_it = _streams.find(info->GetId());
		if (stream_it == _streams.end())
		{
			// Sometimes stream rejects stream creation if the input codec is not supported. So this is a normal situation.
			logtd("OnStreamUpdated failed. Cannot find stream : %s/%u", info->GetName().CStr(), info->GetId());
			return true;
		}

		auto stream = stream_it->second;

		lock.unlock();

		return stream->OnStreamUpdated(info);
	}

	std::shared_ptr<ApplicationWorker> Application::GetWorkerByStreamID(info::stream_id_t stream_id)
	{
		if (_application_worker_count == 0)
		{
			return nullptr;
		}

		std::shared_lock<std::shared_mutex> worker_lock(_application_worker_lock);
		return _application_workers[stream_id % _application_worker_count];
	}

	bool Application::OnSendFrame(const std::shared_ptr<info::Stream> &stream,
								  const std::shared_ptr<MediaPacket> &media_packet)
	{
		auto application_worker = GetWorkerByStreamID(stream->GetId());
		if (application_worker == nullptr)
		{
			return false;
		}

		return application_worker->PushMediaPacket(GetStream(stream->GetId()), media_packet);
	}

	uint32_t Application::GetStreamCount()
	{
		return _streams.size();
	}

	std::shared_ptr<Stream> Application::GetStream(uint32_t stream_id)
	{
		std::shared_lock<std::shared_mutex> lock(_stream_map_mutex);
		auto it = _streams.find(stream_id);
		if (it == _streams.end())
		{
			return nullptr;
		}

		return it->second;
	}

	std::shared_ptr<Stream> Application::GetStream(ov::String stream_name)
	{
		std::shared_lock<std::shared_mutex> lock(_stream_map_mutex);
		for (auto const &x : _streams)
		{
			auto stream = x.second;
			if (stream->GetName() == stream_name)
			{
				return stream;
			}
		}

		return nullptr;
	}

	PushApplication::PushApplication(const std::shared_ptr<Publisher> &publisher, const info::Application &application_info)
		: Application(publisher, application_info),
		  _session_control_stop_thread_flag(true)
	{
	}

	bool PushApplication::Start()
	{
		_session_control_stop_thread_flag = false;
		_session_contol_thread = std::thread(&PushApplication::SessionControlThread, this);
		pthread_setname_np(_session_contol_thread.native_handle(), "PushSessionCtrl");

		return Application::Start();
	}

	bool PushApplication::Stop()
	{
		if (_session_control_stop_thread_flag == false)
		{
			_session_control_stop_thread_flag = true;
			if (_session_contol_thread.joinable())
			{
				_session_contol_thread.join();
			}
		}

		return Application::Stop();
	}

	std::shared_ptr<info::Push> PushApplication::GetPushInfoById(ov::String id)
	{
		std::lock_guard<std::shared_mutex> lock(_push_map_mutex);
		auto it = _pushes.find(id);
		if (it == _pushes.end())
		{
			return nullptr;
		}

		return it->second;
	}

	std::shared_ptr<ov::Error> PushApplication::StartPush(const std::shared_ptr<info::Push> &push)
	{
		// Validation check for duplicate id
		if (GetPushInfoById(push->GetId()) != nullptr)
		{
			ov::String error_message = "Duplicate ID";
			return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureDuplicateKey, error_message);
		}

		// 녹화 활성화
		push->SetEnable(true);
		push->SetRemove(false);

		std::unique_lock<std::shared_mutex> lock(_push_map_mutex);
		_pushes[push->GetId()] = push;

		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::Success, "Success");
	}

	std::shared_ptr<ov::Error> PushApplication::StopPush(const std::shared_ptr<info::Push> &push)
	{
		if (push->GetId().IsEmpty() == true)
		{
			ov::String error_message = "There is no required parameter [";

			if (push->GetId().IsEmpty() == true)
			{
				error_message += " id";
			}

			error_message += "]";

			return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureInvalidParameter, error_message);
		}

		auto push_info = GetPushInfoById(push->GetId());
		if (push_info == nullptr)
		{
			ov::String error_message = ov::String::FormatString("There is no push information related to the ID [%s]", push->GetId().CStr());

			return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::FailureNotExist, error_message);
		}

		push_info->SetEnable(false);
		push_info->SetRemove(true);

		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::Success, "Success");
	}

	std::shared_ptr<ov::Error> PushApplication::GetPushes(const std::shared_ptr<info::Push> push, std::vector<std::shared_ptr<info::Push>> &results)
	{
		std::lock_guard<std::shared_mutex> lock(_push_map_mutex);

		for (auto &[id, push] : _pushes)
		{
			if (!push->GetId().IsEmpty() && push->GetId() != id)
				continue;

			results.push_back(push);
		}

		return ov::Error::CreateError(PUSH_PUBLISHER_ERROR_DOMAIN, ErrorCode::Success, "Success");
	}

	void PushApplication::StartPushInternal(const std::shared_ptr<info::Push> &push, std::shared_ptr<pub::Session> session)
	{
		// Check the status of the session.
		auto prev_session_state = session->GetState();

		switch (prev_session_state)
		{
			// State of disconnected and ready to connect
			case pub::Session::SessionState::Ready:
				[[fallthrough]];
			// State of stopped
			case pub::Session::SessionState::Stopped:
				[[fallthrough]];
			// State of failed (connection refused, disconnected)
			case pub::Session::SessionState::Error:
				logti("Push started. %s", push->GetInfoString().CStr());
				session->Start();
				break;
			// State of Started
			case pub::Session::SessionState::Started:
				[[fallthrough]];
			// State of Stopping
			case pub::Session::SessionState::Stopping:
				break;
		}

		auto session_state = session->GetState();
		if (prev_session_state != session_state)
		{
			logtd("Changed push state. (%d - %d)", prev_session_state, session_state);
		}
	}

	void PushApplication::StopPushInternal(const std::shared_ptr<info::Push> &push, std::shared_ptr<pub::Session> session)
	{
		auto prev_session_state = session->GetState();

		switch (prev_session_state)
		{
			case pub::Session::SessionState::Started:
				session->Stop();
				logti("Push ended. %s", push->GetInfoString().CStr());
				break;
			case pub::Session::SessionState::Ready:
				[[fallthrough]];
			case pub::Session::SessionState::Stopping:
				[[fallthrough]];
			case pub::Session::SessionState::Stopped:
				[[fallthrough]];
			case pub::Session::SessionState::Error:
				break;
		}

		auto session_state = session->GetState();
		if (prev_session_state != session_state)
		{
			logtd("Changed push state. (%d - %d)", prev_session_state, session_state);
		}
	}

	void PushApplication::SessionControlThread()
	{
		ov::StopWatch	timer;
		int64_t timer_interval = 1000;
		timer.Start();

		while (!_session_control_stop_thread_flag)
		{
			if (timer.IsElapsed(timer_interval) && timer.Update())
			{
				// list of Push to be deleted
				std::vector<std::shared_ptr<info::Push>> remove_pushes;

				if (true)
				{
					std::lock_guard<std::shared_mutex> lock(_push_map_mutex);
					for (auto &[id, push] : _pushes)
					{
						// If it is a removed push job, add to the remove waiting list
						if (push->GetRemove() == true)
						{
							remove_pushes.push_back(push);
						}

						// Find a stream by stream name
						auto stream = GetStream(push->GetStreamName());
						if (stream == nullptr || stream->GetState() != pub::Stream::State::STARTED)
						{
							logtd("There is no stream for Push or it has not started. %s", push->GetInfoString().CStr());
							push->SetState(info::Push::PushState::Ready);
							continue;
						}

						// Find a session by session ID
						auto session = stream->GetSession(push->GetSessionId());
						if (session == nullptr)
						{
							session = stream->CreatePushSession(push);
							if (session == nullptr)
							{
								logte("Could not create session");
								continue;
							}

							push->SetSessionId(session->GetId());
						}

						// Starts only in the enabled state and stops otherwise
						if (push->GetEnable() == true && push->GetRemove() == false)
						{
							StartPushInternal(push, session);
						}
						else
						{
							StopPushInternal(push, session);
						}
					}
				}

				if (true)
				{
					std::unique_lock<std::shared_mutex> lock(_push_map_mutex);
					for (auto &push : remove_pushes)
					{
						auto stream = GetStream(push->GetStreamName());
						if (stream != nullptr)
						{
							stream->RemoveSession(push->GetSessionId());
						}

						_pushes.erase(push->GetId());
					}
				}
			}

			usleep(100 * 1000);	 // 100ms
		}
	}
}  // namespace pub
