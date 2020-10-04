#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>

#include <modules/rtmp/rtmp_writer.h>

#include "monitoring/monitoring.h"

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

	void PushStart();
	void PushStop();
	void PushStat();
private:
	bool Start() override;
	bool Stop() override;

	std::shared_ptr<RtmpWriter>				_writer;

	std::shared_ptr<mon::StreamMetrics>		_stream_metrics;	

};
