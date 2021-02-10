#include "publisher.h"
#include "application.h"
#include "publisher_private.h"
#include <algorithm>

namespace pub
{
	ApplicationWorker::ApplicationWorker(uint32_t worker_id, ov::String worker_name)
		: _stream_data_queue(nullptr, 500),
		_incoming_packet_queue(nullptr, 500)
	{
		_worker_id = worker_id;
		_worker_name = worker_name;
		_stop_thread_flag = false;
	}

	bool ApplicationWorker::Start()
	{
		_stop_thread_flag = false;
		_worker_thread = std::thread(&ApplicationWorker::WorkerThread, this);
		pthread_setname_np(_worker_thread.native_handle(), "AppWorker");

		ov::String queue_name;

		queue_name.Format("%s - Stream Data Queue", _worker_name.CStr());
		_stream_data_queue.SetAlias(queue_name.CStr());

		queue_name.Format("%s - Incoming Packet Queue", _worker_name.CStr());
		_incoming_packet_queue.SetAlias(queue_name.CStr());

		logti("%s ApplicationWorker has been created", _worker_name.CStr());

		return true;
	}

	bool ApplicationWorker::Stop()
	{
		if(_stop_thread_flag == true)
		{
			return true;
		}

		_stream_data_queue.Clear();
		_incoming_packet_queue.Clear();

		_stop_thread_flag = true;

		_queue_event.Notify();

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

	bool ApplicationWorker::PushNetworkPacket(const std::shared_ptr<Session> &session, const std::shared_ptr<const ov::Data> &data)
	{
		auto packet = std::make_shared<ApplicationWorker::IncomingPacket>(session, data);
		_incoming_packet_queue.Enqueue(std::move(packet));

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
		if(data.has_value())
		{
			return data.value();
		}

		return nullptr;
	}

	std::shared_ptr<ApplicationWorker::IncomingPacket> ApplicationWorker::PopIncomingPacket()
	{
		if (_incoming_packet_queue.IsEmpty())
		{
			return nullptr;
		}

		auto data = _incoming_packet_queue.Dequeue(0);
		if(data.has_value())
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
				if(stream_data->_media_packet->GetMediaType() == cmn::MediaType::Video)
				{
					stream_data->_stream->SendVideoFrame(stream_data->_media_packet);
				}
				else if(stream_data->_media_packet->GetMediaType() == cmn::MediaType::Audio)
				{
					stream_data->_stream->SendAudioFrame(stream_data->_media_packet);
				}
				else
				{
					// Nothing can do
				}
			}

			// Check incoming packet is available
			std::shared_ptr<IncomingPacket> packet = PopIncomingPacket();
			if (packet)
			{
				packet->_session->OnPacketReceived(packet->_session, packet->_data);
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

	const char* Application::GetApplicationTypeName()
	{
		if(_publisher == nullptr)
		{
			return "";
		}

		if(_app_type_name.IsEmpty())
		{
			_app_type_name.Format("%s %s",  _publisher->GetPublisherName(), "Application");
		}

		return _app_type_name.CStr();
	}

	bool Application::Start()
	{
		_application_worker_count = GetConfig().GetStreamLoadBalancingThreadCount();
		if(_application_worker_count < MIN_APPLICATION_WORKER_COUNT)
		{
			_application_worker_count = MIN_APPLICATION_WORKER_COUNT;
		}
		if(_application_worker_count > MAX_APPLICATION_WORKER_COUNT)
		{
			_application_worker_count = MIN_APPLICATION_WORKER_COUNT;
		}
		
		std::lock_guard<std::shared_mutex> worker_lock(_application_worker_lock);

		for(uint32_t i = 0; i < _application_worker_count; i++)
		{
			auto worker_name = ov::String::FormatString("%s/%s/%d", GetApplicationTypeName(), GetName().CStr(), i);
			auto app_worker = std::make_shared<ApplicationWorker>(i, worker_name.CStr());
			if (app_worker->Start() == false)
			{
				logte("Cannot create ApplicationWorker (%s)", worker_name.CStr());
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
		for(const auto &worker : _application_workers)
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

		for(const auto &x : _streams)
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
		auto stream_worker_count = GetConfig().GetSessionLoadBalancingThreadCount();

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
		if(stream_it == _streams.end())
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
		if(stream_it == _streams.end())
		{
			// Sometimes stream rejects stream creation if the input codec is not supported. So this is a normal situation.
			logtd("OnStreamPrepared failed. Cannot find stream : %s/%u", info->GetName().CStr(), info->GetId());
			return true;
		}

		auto stream = stream_it->second;

		lock.unlock();

		// Start stream
		stream->Start();

		return true;
	}

	std::shared_ptr<ApplicationWorker> Application::GetWorkerByStreamID(info::stream_id_t stream_id)
	{
		if(_application_worker_count == 0)
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
		if(application_worker == nullptr)
		{
			return false;
		}

		return application_worker->PushMediaPacket(GetStream(stream->GetId()), media_packet);
	}
	

	bool Application::PushIncomingPacket(const std::shared_ptr<info::Session> &session_info,
										 const std::shared_ptr<const ov::Data> &data)
	{
		auto stream_id = session_info->GetStream().GetId();
		auto application_worker = GetWorkerByStreamID(stream_id);
		if(application_worker == nullptr)
		{
			return false;
		}

		return application_worker->PushNetworkPacket(std::static_pointer_cast<Session>(session_info), data);
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
}  // namespace pub