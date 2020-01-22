//
// Created by getroot on 19. 12. 12.
//
#pragma once

#include <base/media_route/media_buffer.h>
#include "ovt_packet.h"
#include "ovt_packetizer_interface.h"

#define INIT_PAYLOAD_BUFFER_SIZE	1024 * 1024 * 1024 		// 1MB

class OvtDepacketizer
{
public:
	OvtDepacketizer();
	~OvtDepacketizer();

	bool AppendPacket(const std::shared_ptr<OvtPacket> &packet);
	bool IsAvaliableMediaPacket();
	const std::shared_ptr<MediaPacket> PopMediaPacket();

private:
	ov::Data									_payload_buffer;
	std::queue<std::shared_ptr<MediaPacket>>	_media_packets;
};
