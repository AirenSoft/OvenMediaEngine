#pragma once

#include <base/media_route/media_buffer.h>
#include "ovt_packet.h"
#include "ovt_packetizer_interface.h"

class OvtPacketizer
{
public:
	OvtPacketizer(const std::shared_ptr<OvtPacketizerInterface> &stream);
	~OvtPacketizer();

	// Packetizing the MediaPacket
	bool Packetize(uint64_t timestamp, const std::shared_ptr<MediaPacket> &media_packet);
	// For Extension
	bool Packetize(uint8_t payload_type, uint64_t timestamp, const std::shared_ptr<ov::Data> &packet);

private:
	uint16_t 									_sequence_number;
	std::shared_ptr<OvtPacketizerInterface> 	_stream;
};
