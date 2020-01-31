#pragma once

#include <utility>
#include "base/common_types.h"
#include "base/info/stream_info.h"
#include "base/info/session_info.h"
#include "base/media_route/media_route_application_observer.h"
#include "base/ovlibrary/semaphore.h"
#include "base/ovlibrary/string.h"
#include "config/config.h"
#include "stream.h"

namespace pub
{
	enum ApplicationState
	{
		Idle,
		Started,
		Stopped,
		Error
	};

	class Application : public info::Application, public MediaRouteApplicationObserver
	{
	public:
		// MediaRouteApplicationObserver Implementation
		bool OnCreateStream(const std::shared_ptr<info::StreamInfo> &info) override;
		bool OnDeleteStream(const std::shared_ptr<info::StreamInfo> &info) override;

		// Queue에 데이터를 넣는다.
		bool OnSendVideoFrame(const std::shared_ptr<info::StreamInfo> &stream_info,
							  const std::shared_ptr<MediaPacket> &media_packet) override;

		bool OnSendAudioFrame(const std::shared_ptr<info::StreamInfo> &stream_info,
							  const std::shared_ptr<MediaPacket> &media_packet) override;

		// 수신된 Network Packet을 Application에 넣고 처리를 기다린다.
		bool PushIncomingPacket(const std::shared_ptr<info::SessionInfo> &session_info,
								const std::shared_ptr<const ov::Data> &data);

		std::shared_ptr<Stream> GetStream(uint32_t stream_id);
		std::shared_ptr<Stream> GetStream(ov::String stream_name);

		virtual bool Start();
		virtual bool Stop();

	protected:
		explicit Application(const info::Application &application_info);
		virtual ~Application();

		// Stream에 VideoFrame을 전송한다.
		// virtual로 Child에서 원하면 다른 작업을 할 수 있게 한다.
		virtual void SendVideoFrame(const std::shared_ptr<info::StreamInfo> &stream_info,
									const std::shared_ptr<MediaPacket> &media_packet);

		virtual void SendAudioFrame(const std::shared_ptr<info::StreamInfo> &info,
									const std::shared_ptr<MediaPacket> &media_packet);

		virtual void OnPacketReceived(const std::shared_ptr<info::SessionInfo> &session_info, const std::shared_ptr<const ov::Data> &data);

		std::map<uint32_t, std::shared_ptr<Stream>> _streams;

	private:
		void WorkerThread();
		// For child, 실제 구현부는 자식에서 처리한다.

		// Stream을 자식을 통해 생성해서 받는다.
		virtual std::shared_ptr<Stream> CreateStream(const std::shared_ptr<info::StreamInfo> &info, uint32_t thread_count) = 0;
		virtual bool DeleteStream(const std::shared_ptr<info::StreamInfo> &info) = 0;

		// Audio Stream 전달 Interface를 구현해야 함
		//virtual void						OnAudioFrame(int32_t stream_id) = 0;

		// Queue에 넣을 정제된 데이터 구조
		// std::tuple을 이용하려 했으나, return에 nullptr을 사용할 수 없어서 관둠
		class VideoStreamData
		{
		public:
			VideoStreamData(const std::shared_ptr<info::StreamInfo> &stream_info,
							const std::shared_ptr<MediaPacket> &media_packet)
			{
				_stream_info = stream_info;
				_media_packet = media_packet;
			}

			std::shared_ptr<info::StreamInfo> _stream_info;
			std::shared_ptr<MediaPacket> _media_packet;
		};
		std::shared_ptr<Application::VideoStreamData> PopVideoStreamData();

		class AudioStreamData
		{
		public:
			AudioStreamData(const std::shared_ptr<info::StreamInfo> &stream_info,
							const std::shared_ptr<MediaPacket> &media_packet)
			{
				_stream_info = stream_info;
				_media_packet = media_packet;
			}

			std::shared_ptr<info::StreamInfo> _stream_info;
			std::shared_ptr<MediaPacket> _media_packet;
		};
		std::shared_ptr<Application::AudioStreamData> PopAudioStreamData();

		class IncomingPacket
		{
		public:
			IncomingPacket(const std::shared_ptr<info::SessionInfo> &session_info,
						   const std::shared_ptr<const ov::Data> &data)
			{
				_session_info = session_info;
				_data = data;
			}

			std::shared_ptr<info::SessionInfo> _session_info;
			std::shared_ptr<const ov::Data> _data;
		};
		std::shared_ptr<Application::IncomingPacket> PopIncomingPacket();

		bool _stop_thread_flag;
		std::thread _worker_thread;
		ov::Semaphore _queue_event;

		std::queue<std::shared_ptr<VideoStreamData>> _video_stream_queue;
		std::mutex _video_stream_queue_guard;

		std::queue<std::shared_ptr<AudioStreamData>> _audio_stream_queue;
		std::mutex _audio_stream_queue_guard;

		std::queue<std::shared_ptr<IncomingPacket>> _incoming_packet_queue;
		std::mutex _incoming_packet_queue_guard;

		//std::queue<std::shared_ptr<AudioStreamData>>	_audio_stream_queue;
	};
}  // namespace pub