#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>
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
	void SendDataFrame(const std::shared_ptr<MediaPacket> &media_packet) override {} // Not supported

	std::shared_ptr<pub::Session> CreatePushSession(std::shared_ptr<info::Push> &push) override;

private:
	bool Start() override;
	bool Stop() override;

	std::shared_ptr<mon::StreamMetrics> _stream_metrics;
};
