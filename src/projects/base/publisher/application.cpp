#include "publisher.h"
#include "application.h"
#include "publisher_private.h"
#include <algorithm>

namespace pub
{
	Application::Application(const std::shared_ptr<Publisher> &publisher, const info::Application &application_info)
		: info::Application(application_info),
		_video_stream_queue(nullptr, 100),
		_audio_stream_queue(nullptr, 100),
		_incoming_packet_queue(nullptr, 100)
	{
		_publisher = publisher;
		_stop_thread_flag = false;
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
		// Thread 생성
		_stop_thread_flag = false;
		_worker_thread = std::thread(&Application::WorkerThread, this);

		ov::String queue_name;

		queue_name.Format("%s/%s - Video Queue", GetApplicationTypeName(), GetName().CStr());
		_video_stream_queue.SetAlias(queue_name.CStr());

		queue_name.Format("%s/%s - Audio Queue", GetApplicationTypeName(), GetName().CStr());
		_audio_stream_queue.SetAlias(queue_name.CStr());

		queue_name.Format("%s/%s - Incoming Queue", GetApplicationTypeName(), GetName().CStr());
		_incoming_packet_queue.SetAlias(queue_name.CStr());

		logti("%s has created [%s] application", GetApplicationTypeName(), GetName().CStr());

		return true;
	}

	bool Application::Stop()
	{
		if(_stop_thread_flag == true)
		{
			return true;
		}
		_stop_thread_flag = true;
		_queue_event.Notify();

		if (_worker_thread.joinable())
		{
			_worker_thread.join();
		}

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

	// Call by MediaRouteApplicationObserver
	// Stream이 생성되었을 때 호출된다.
	bool Application::OnCreateStream(const std::shared_ptr<info::Stream> &info)
	{
		// Stream을 자식을 통해 생성해서 연결한다.
		auto worker_count = GetConfig().GetThreadCount();
		auto stream = CreateStream(info, worker_count);

		if (!stream)
		{
			// Stream 생성 실패
			return false;
		}

		std::lock_guard<std::shared_mutex> lock(_stream_map_mutex);
		_streams[info->GetId()] = stream;

		return true;
	}

	bool Application::OnDeleteStream(const std::shared_ptr<info::Stream> &info)
	{
		std::unique_lock<std::shared_mutex> lock(_stream_map_mutex);

		auto stream_it = _streams.find(info->GetId());
		if(stream_it == _streams.end())
		{
			logte("OnDeleteStream failed. Cannot find stream : %s/%u", info->GetName().CStr(), info->GetId());
			return false;
		}

		auto stream = stream_it->second;

		lock.unlock();

		// Stream이 삭제되었음을 자식에게 알려서 처리하게 함
		if (DeleteStream(info) == false)
		{
			return false;
		}

		lock.lock();
		_streams.erase(info->GetId());
		stream->Stop();

		return true;
	}

	bool Application::OnSendVideoFrame(const std::shared_ptr<info::Stream> &stream,
									   const std::shared_ptr<MediaPacket> &media_packet)
	{
		auto data = std::make_shared<Application::VideoStreamData>(stream, media_packet);
		_video_stream_queue.Enqueue(std::move(data));
		_last_video_ts_ms = media_packet->GetPts() * stream->GetTrack(media_packet->GetTrackId())->GetTimeBase().GetExpr() * 1000;

		_queue_event.Notify();

		return true;
	}

	bool Application::OnSendAudioFrame(const std::shared_ptr<info::Stream> &stream,
									   const std::shared_ptr<MediaPacket> &media_packet)
	{
		auto data = std::make_shared<Application::AudioStreamData>(stream, media_packet);

		_audio_stream_queue.Enqueue(std::move(data));
		_last_audio_ts_ms = media_packet->GetPts() * stream->GetTrack(media_packet->GetTrackId())->GetTimeBase().GetExpr() * 1000;

		_queue_event.Notify();

		return true;
	}

	bool Application::PushIncomingPacket(const std::shared_ptr<info::Session> &session_info,
										 const std::shared_ptr<const ov::Data> &data)
	{
		auto packet = std::make_shared<Application::IncomingPacket>(session_info, data);
		_incoming_packet_queue.Enqueue(std::move(packet));

		_queue_event.Notify();

		return true;
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

	std::shared_ptr<Application::VideoStreamData> Application::PopVideoStreamData()
	{
		if (_video_stream_queue.IsEmpty())
		{
			return nullptr;
		}

		auto data = _video_stream_queue.Dequeue();
		if(data.has_value())
		{
			return data.value();
		}
		
		return nullptr;
	}

	std::shared_ptr<Application::AudioStreamData> Application::PopAudioStreamData()
	{
		if (_audio_stream_queue.IsEmpty())
		{
			return nullptr;
		}

		auto data = _audio_stream_queue.Dequeue();
		if(data.has_value())
		{
			return data.value();
		}
		
		return nullptr;
	}

	std::shared_ptr<Application::IncomingPacket> Application::PopIncomingPacket()
	{
		if (_incoming_packet_queue.IsEmpty())
		{
			return nullptr;
		}

		auto data = _incoming_packet_queue.Dequeue();
		if(data.has_value())
		{
			return data.value();
		}
		
		return nullptr;
	}

	void Application::WorkerThread()
	{
		ov::StopWatch stat_stop_watch;
		stat_stop_watch.Start();

		while (!_stop_thread_flag)
		{
			if (stat_stop_watch.IsElapsed(5000) && stat_stop_watch.Update())
			{
				logts("Stats for publisher queue [%s(%u)]: VQ: %zu, AQ: %zu, Incoming Q: %zu",
					  _app_config.GetName().CStr(), _application_id,
					  _video_stream_queue.Size(),
					  _audio_stream_queue.Size(),
					  _incoming_packet_queue.Size());
			}

			_queue_event.Wait();

			// Check video data is available
			std::shared_ptr<Application::VideoStreamData> video_data = PopVideoStreamData();

			if ((video_data != nullptr) && (video_data->_stream != nullptr) && (video_data->_media_packet != nullptr))
			{
				SendVideoFrame(video_data->_stream, video_data->_media_packet);
			}

			// Check audio data is available
			std::shared_ptr<Application::AudioStreamData> audio_data = PopAudioStreamData();

			if ((audio_data != nullptr) && (audio_data->_stream != nullptr) && (audio_data->_media_packet != nullptr))
			{
				SendAudioFrame(audio_data->_stream, audio_data->_media_packet);
			}

			// Check incoming packet is available
			std::shared_ptr<IncomingPacket> packet = PopIncomingPacket();
			if (packet)
			{
				OnPacketReceived(packet->_session_info, packet->_data);
			}
		}
	}

	void Application::SendVideoFrame(const std::shared_ptr<info::Stream> &stream_info, const std::shared_ptr<MediaPacket> &media_packet)
	{
		// Stream에 Packet을 전송한다.
		auto stream = GetStream(stream_info->GetId());
		if (!stream)
		{
			// stream을 찾을 수 없다.
			return;
		}

		stream->SendVideoFrame(media_packet);
	}

	void Application::SendAudioFrame(const std::shared_ptr<info::Stream> &stream_info, const std::shared_ptr<MediaPacket> &media_packet)
	{
		// Stream에 Packet을 전송한다.
		auto stream = GetStream(stream_info->GetId());
		if (!stream)
		{
			// stream을 찾을 수 없다.
			return;
		}

		stream->SendAudioFrame(media_packet);
	}

	void Application::OnPacketReceived(const std::shared_ptr<info::Session> &session_info, const std::shared_ptr<const ov::Data> &data)
	{
		// Stream으로 갈 필요없이 바로 Session으로 간다.
		// Stream은 Broad하게 전송할때만 필요하다.
		auto session = std::static_pointer_cast<Session>(session_info);
		session->OnPacketReceived(session_info, data);
	}
}  // namespace pub