#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>
#include <modules/mpegts/mpegts_writer.h>

#include "monitoring/monitoring.h"
#include "mpegtspush_session.h"

class MpegtsPushStream : public pub::Stream
{
public:
	static std::shared_ptr<MpegtsPushStream> Create(const std::shared_ptr<pub::Application> application,
												  const info::Stream &info);

	explicit MpegtsPushStream(const std::shared_ptr<pub::Application> application,
							const info::Stream &info);
	~MpegtsPushStream() final;

	void SendFrame(const std::shared_ptr<MediaPacket> &media_packet);
	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	std::shared_ptr<MpegtsPushSession> CreateSession();
	bool DeleteSession(uint32_t session_id);

private:
	bool Start() override;
	bool Stop() override;

	ov::StopWatch _stop_watch;
	std::shared_ptr<mon::StreamMetrics> _stream_metrics;
};
