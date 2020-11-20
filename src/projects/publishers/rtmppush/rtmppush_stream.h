#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>

#include "monitoring/monitoring.h"

#include "rtmppush_session.h"
#include <modules/rtmp/rtmp_writer.h>

class RtmpPushStream : public pub::Stream
{
public:
	static std::shared_ptr<RtmpPushStream> Create(const std::shared_ptr<pub::Application> application,
											 const info::Stream &info);

	explicit RtmpPushStream(const std::shared_ptr<pub::Application> application,
					   const info::Stream &info);
	~RtmpPushStream() final;

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	std::shared_ptr<RtmpPushSession> CreateSession();
	bool DeleteSession(uint32_t session_id);

private:
	bool Start() override;
	bool Stop() override;


	std::shared_ptr<mon::StreamMetrics>		_stream_metrics;	

	std::shared_ptr<RtmpWriter>				_writer;

};
