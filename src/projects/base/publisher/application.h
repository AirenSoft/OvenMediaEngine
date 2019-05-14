#pragma once

#include <utility>
#include "base/common_types.h"
#include "base/ovlibrary/string.h"
#include "base/ovlibrary/semaphore.h"
#include "config/config.h"
#include "base/application/stream_info.h"
#include "base/media_route/media_route_application_observer.h"
#include "stream.h"

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
	bool OnCreateStream(std::shared_ptr<StreamInfo> info) override;
	bool OnDeleteStream(std::shared_ptr<StreamInfo> info) override;

	// Queue에 데이터를 넣는다.
	bool OnSendVideoFrame(std::shared_ptr<StreamInfo> stream_info,
	                      std::shared_ptr<MediaTrack> track,
	                      std::unique_ptr<EncodedFrame> encoded_frame,
	                      std::unique_ptr<CodecSpecificInfo> codec_info,
	                      std::unique_ptr<FragmentationHeader> fragmentation) override;

	bool OnSendAudioFrame(std::shared_ptr<StreamInfo> stream_info,
	                      std::shared_ptr<MediaTrack> track,
	                      std::unique_ptr<EncodedFrame> encoded_frame,
	                      std::unique_ptr<CodecSpecificInfo> codec_info,
	                      std::unique_ptr<FragmentationHeader> fragmentation) override;

	// 수신된 Network Packet을 Application에 넣고 처리를 기다린다.
	bool PushIncomingPacket(std::shared_ptr<SessionInfo> session_info,
	                        std::shared_ptr<const ov::Data> data);

	std::shared_ptr<Stream> GetStream(uint32_t stream_id);
	std::shared_ptr<Stream> GetStream(ov::String stream_name);

protected:
	explicit Application(const info::Application *application_info);
	virtual ~Application();

	virtual bool Start();
	virtual bool Stop();

	// Stream에 VideoFrame을 전송한다.
	// virtual로 Child에서 원하면 다른 작업을 할 수 있게 한다.
	virtual void SendVideoFrame(std::shared_ptr<StreamInfo> info,
	                            std::shared_ptr<MediaTrack> track,
	                            std::unique_ptr<EncodedFrame> encoded_frame,
	                            std::unique_ptr<CodecSpecificInfo> codec_info,
	                            std::unique_ptr<FragmentationHeader> fragmentation);

	virtual void SendAudioFrame(std::shared_ptr<StreamInfo> info,
	                            std::shared_ptr<MediaTrack> track,
	                            std::unique_ptr<EncodedFrame> encoded_frame,
	                            std::unique_ptr<CodecSpecificInfo> codec_info,
	                            std::unique_ptr<FragmentationHeader> fragmentation);

	virtual void OnPacketReceived(std::shared_ptr<SessionInfo> session_info, std::shared_ptr<const ov::Data> data);

	std::map<uint32_t, std::shared_ptr<Stream>> _streams;

private:
	void WorkerThread();
	// For child, 실제 구현부는 자식에서 처리한다.

	// Stream을 자식을 통해 생성해서 받는다.
	virtual std::shared_ptr<Stream> CreateStream(std::shared_ptr<StreamInfo> info, uint32_t thread_count) = 0;
	virtual bool DeleteStream(std::shared_ptr<StreamInfo> info) = 0;

	// Audio Stream 전달 Interface를 구현해야 함
	//virtual void						OnAudioFrame(int32_t stream_id) = 0;

	// Queue에 넣을 정제된 데이터 구조
	// std::tuple을 이용하려 했으나, return에 nullptr을 사용할 수 없어서 관둠
	class VideoStreamData
	{
	public:
		VideoStreamData(std::shared_ptr<StreamInfo> stream_info,
		                std::shared_ptr<MediaTrack> track,
		                std::unique_ptr<EncodedFrame> encoded_frame,
		                std::unique_ptr<CodecSpecificInfo> codec_info,
		                std::unique_ptr<FragmentationHeader> fragmentation)
		{
			_stream_info = std::move(stream_info);
			_track = std::move(track);
			_encoded_frame = std::move(encoded_frame);
			_codec_info = std::move(codec_info);
			_framgmentation_header = std::move(fragmentation);
		}

		std::shared_ptr<StreamInfo> _stream_info;
		std::shared_ptr<MediaTrack> _track;
		std::unique_ptr<EncodedFrame> _encoded_frame;
		std::unique_ptr<CodecSpecificInfo> _codec_info;
		std::unique_ptr<FragmentationHeader> _framgmentation_header;
	};
	std::unique_ptr<Application::VideoStreamData> PopVideoStreamData();

	class AudioStreamData
	{
	public:
		AudioStreamData(std::shared_ptr<StreamInfo> stream_info,
		                std::shared_ptr<MediaTrack> track,
		                std::unique_ptr<EncodedFrame> encoded_frame,
		                std::unique_ptr<CodecSpecificInfo> codec_info,
		                std::unique_ptr<FragmentationHeader> fragmentation)
		{
			_stream_info = std::move(stream_info);
			_track = std::move(track);
			_encoded_frame = std::move(encoded_frame);
			_codec_info = std::move(codec_info);
			_framgmentation_header = std::move(fragmentation);
		}

		std::shared_ptr<StreamInfo> _stream_info;
		std::shared_ptr<MediaTrack> _track;
		std::unique_ptr<EncodedFrame> _encoded_frame;
		std::unique_ptr<CodecSpecificInfo> _codec_info;
		std::unique_ptr<FragmentationHeader> _framgmentation_header;
	};
	std::unique_ptr<Application::AudioStreamData> PopAudioStreamData();

	class IncomingPacket
	{
	public:
		IncomingPacket(std::shared_ptr<SessionInfo> session_info, std::shared_ptr<const ov::Data> data)
		{
			_session_info = std::move(session_info);
			_data = std::move(data);
		}

		std::shared_ptr<SessionInfo> _session_info;
		std::shared_ptr<const ov::Data> _data;
	};
	std::unique_ptr<Application::IncomingPacket> PopIncomingPacket();

	bool _stop_thread_flag;
	std::thread _worker_thread;
	ov::Semaphore _queue_event;

	std::queue<std::unique_ptr<VideoStreamData>> _video_stream_queue;
	std::mutex _video_stream_queue_guard;

	std::queue<std::unique_ptr<AudioStreamData>> _audio_stream_queue;
	std::mutex _audio_stream_queue_guard;

	std::queue<std::unique_ptr<IncomingPacket>> _incoming_packet_queue;
	std::mutex _incoming_packet_queue_guard;

	//std::queue<std::unique_ptr<AudioStreamData>>	_audio_stream_queue;

};
