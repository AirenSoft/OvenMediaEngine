#include "publisher_private.h"
#include "application.h"

#include <algorithm>

Application::Application(const info::Application &application_info)
	: info::Application(application_info)
{
	_stop_thread_flag = false;
}

Application::~Application()
{
	Stop();
}

bool Application::Start()
{
	// Thread 생성
	_stop_thread_flag = false;
	_worker_thread = std::thread(&Application::WorkerThread, this);

	return true;
}

bool Application::Stop()
{
	_stop_thread_flag = true;
	_worker_thread.join();

	return true;
}

// Call by MediaRouteApplicationObserver
// Stream이 생성되었을 때 호출된다.
bool Application::OnCreateStream(const std::shared_ptr<StreamInfo> &info)
{
	// Stream을 자식을 통해 생성해서 연결한다.
	auto worker_count = GetThreadCount();
	auto stream = CreateStream(info, worker_count);

	if(!stream)
	{
		// Stream 생성 실패
		return false;
	}

	_streams[info->GetId()] = stream;

	return true;
}

bool Application::OnDeleteStream(const std::shared_ptr<StreamInfo> &info)
{
	if(_streams.count(info->GetId()) <= 0)
	{
		logte("OnDeleteStream failed. Cannot find stream : %s/%u", info->GetName().CStr(), info->GetName().CStr());
		return false;
	}

	auto stream = std::static_pointer_cast<Stream>(GetStream(info->GetId()));
	if(stream == nullptr)
	{
		logte("OnDeleteStream failed. Cannot find stream : %s/%u", info->GetName().CStr(), info->GetName().CStr());
		return false;
	}

	// Stream이 삭제되었음을 자식에게 알려서 처리하게 함
	if(DeleteStream(info) == false)
	{
		return false;
	}

	stream->Stop();

	_streams.erase(info->GetId());

	return true;
}

bool Application::OnSendVideoFrame(const std::shared_ptr<StreamInfo> &stream_info,
                                   const std::shared_ptr<MediaPacket> &media_packet)
{
	auto data = std::make_shared<Application::VideoStreamData>(stream_info,
	                                                           media_packet);

	// Mutex (This function may be called by Router thread)
	std::unique_lock<std::mutex> lock(_video_stream_queue_guard);
	_video_stream_queue.push(std::move(data));
	lock.unlock();
	_queue_event.Notify();

	return true;
}

bool Application::OnSendAudioFrame(const std::shared_ptr<StreamInfo> &stream_info,
								   const std::shared_ptr<MediaPacket> &media_packet)
{
	auto data = std::make_shared<Application::AudioStreamData>(stream_info,
	                                                           media_packet);

	// Mutex (This function may be called by Router thread)
	std::unique_lock<std::mutex> lock(_audio_stream_queue_guard);

	_audio_stream_queue.push(std::move(data));
	lock.unlock();
	_queue_event.Notify();

	return true;
}

bool Application::PushIncomingPacket(const std::shared_ptr<SessionInfo> &session_info,
                                     const std::shared_ptr<const ov::Data> &data)
{
	auto packet = std::make_shared<Application::IncomingPacket>(session_info, data);

	// Mutex (This function may be called by IcePort thread)
	std::unique_lock<std::mutex> lock(this->_incoming_packet_queue_guard);
	_incoming_packet_queue.push(std::move(packet));
	lock.unlock();

	_queue_event.Notify();

	return true;
}

std::shared_ptr<Stream> Application::GetStream(uint32_t stream_id)
{
	if(_streams.count(stream_id) <= 0)
	{
		return nullptr;
	}

	return _streams[stream_id];
}

std::shared_ptr<Stream> Application::GetStream(ov::String stream_name)
{
	for(auto const &x : _streams)
	{
		auto stream = x.second;
		if(stream->GetName() == stream_name)
		{
			return stream;
		}
	}

	return nullptr;
}

std::shared_ptr<Application::VideoStreamData> Application::PopVideoStreamData()
{
	std::unique_lock<std::mutex> lock(_video_stream_queue_guard);

	if(_video_stream_queue.empty())
	{
		return nullptr;
	}

	// 데이터를 하나 꺼낸다.
	auto data = std::move(_video_stream_queue.front());
	_video_stream_queue.pop();
	return data;
}

std::shared_ptr<Application::AudioStreamData> Application::PopAudioStreamData()
{
	std::unique_lock<std::mutex> lock(_audio_stream_queue_guard);

	if(_audio_stream_queue.empty())
	{
		return nullptr;
	}

	// 데이터를 하나 꺼낸다.
	auto data = std::move(_audio_stream_queue.front());
	_audio_stream_queue.pop();
	return data;
}

std::shared_ptr<Application::IncomingPacket> Application::PopIncomingPacket()
{
	std::unique_lock<std::mutex> lock(this->_incoming_packet_queue_guard);

	if(_incoming_packet_queue.empty())
	{
		return nullptr;
	}

	// 데이터를 하나 꺼낸다.
	auto packet = std::move(_incoming_packet_queue.front());
	_incoming_packet_queue.pop();
	return packet;
}


/*
 * Application WorkerThread는 Publisher의 Application 마다 하나씩 존재이며, 유일한 Thread이다.
 *
 * 다음과 같은 동작을 수행한다.
 *
 * 1. Router로부터 전달받은 Video/Audio를 Stream에 전달
 * 2. Client로부터 전달받은 Packet을 Stream에 전달
 * 3. 모든 Stream과 Session이 상속받은 Module->Process()를 주기적으로 호출
 *
 */
void Application::WorkerThread()
{
	while(!_stop_thread_flag)
	{
		// Queue에 이벤트가 들어올때까지 무한 대기 한다.
		// TODO: 향후 App 재시작 등의 기능을 위해 WaitFor(time) 기능을 구현한다.
		_queue_event.Wait();

		// Queue에 입력된 데이터를 처리한다.

		// Check video data is available
		std::shared_ptr<Application::VideoStreamData> video_data = PopVideoStreamData();

		if((video_data != nullptr) && (video_data->_stream_info != nullptr) && (video_data->_media_packet != nullptr))
		{
			SendVideoFrame(video_data->_stream_info, video_data->_media_packet);
		}

		// Check audio data is available
		std::shared_ptr<Application::AudioStreamData> audio_data = PopAudioStreamData();

		if((audio_data != nullptr) && (audio_data->_stream_info != nullptr) && (audio_data->_media_packet != nullptr))
		{
			SendAudioFrame(audio_data->_stream_info, audio_data->_media_packet);
		}

		// Check incoming packet is available
		std::shared_ptr<IncomingPacket> packet = PopIncomingPacket();
		if(packet)
		{
			OnPacketReceived(packet->_session_info, packet->_data);
		}

		//TODO: Queue에 입력된 Audio Sample을 처리한다.
		//TODO: ApplicationModule을 호출한다.
	}
}

void Application::SendVideoFrame(const std::shared_ptr<StreamInfo> &stream_info, const std::shared_ptr<MediaPacket> &media_packet)
{
	// Stream에 Packet을 전송한다.
	auto stream = GetStream(stream_info->GetId());
	if(!stream)
	{
		// stream을 찾을 수 없다.
		return;
	}

	stream->SendVideoFrame(media_packet);
}

void Application::SendAudioFrame(const std::shared_ptr<StreamInfo> &stream_info, const std::shared_ptr<MediaPacket> &media_packet)
{
	// Stream에 Packet을 전송한다.
	auto stream = GetStream(stream_info->GetId());
	if(!stream)
	{
		// stream을 찾을 수 없다.
		return;
	}

	stream->SendAudioFrame(media_packet);
}

void Application::OnPacketReceived(const std::shared_ptr<SessionInfo> &session_info, const std::shared_ptr<const ov::Data> &data)
{
	// Stream으로 갈 필요없이 바로 Session으로 간다.
	// Stream은 Broad하게 전송할때만 필요하다.
	auto session = std::static_pointer_cast<Session>(session_info);
	session->OnPacketReceived(session_info, data);
}