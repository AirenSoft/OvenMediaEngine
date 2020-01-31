#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>
#include <modules/ovt_packetizer/ovt_packetizer.h>

class OvtStream : public pub::Stream, public OvtPacketizerInterface
{
public:
	static std::shared_ptr<OvtStream> Create(const std::shared_ptr<pub::Application> application,
											 const StreamInfo &info,
											 uint32_t worker_count);
	explicit OvtStream(const std::shared_ptr<pub::Application> application,
					   const StreamInfo &info);
	~OvtStream() final;

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	bool OnOvtPacketized(std::shared_ptr<OvtPacket> &packet) override;

	bool RemoveSessionByConnectorId(int connector_id);

	Json::Value&		GetDescription();

private:
	bool Start(uint32_t worker_count) override;
	bool Stop() override;

	Json::Value							_description;
	std::shared_ptr<OvtPacketizer>		_packetizer;
};
