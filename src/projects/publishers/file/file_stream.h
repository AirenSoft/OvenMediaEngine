#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>
#include <modules/ovt_packetizer/ovt_packetizer.h>
#include "monitoring/monitoring.h"

#include "file_session.h"

class FileStream : public pub::Stream
{
public:
	static std::shared_ptr<FileStream> Create(const std::shared_ptr<pub::Application> application,
											 const info::Stream &info);

	explicit FileStream(const std::shared_ptr<pub::Application> application,
					   const info::Stream &info);
	~FileStream() final;

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	std::shared_ptr<FileSession> CreateSession();
	bool DeleteSession(uint32_t session_id);

private:
	bool Start() override;
	bool Stop() override;

	std::shared_ptr<mon::StreamMetrics>		_stream_metrics;	
};
