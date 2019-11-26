#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>

class OvtStream : public Stream
{
public:
	static std::shared_ptr<OvtStream> Create(const std::shared_ptr<Application> application,
											 const StreamInfo &info,
											 uint32_t worker_count);
	explicit OvtStream(const std::shared_ptr<Application> application,
					   const StreamInfo &info);
	~OvtStream() final;

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

private:
	bool Start(uint32_t worker_count) override;
	bool Stop() override;
};
