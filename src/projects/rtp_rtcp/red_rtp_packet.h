#pragma once

#include "rtp_packet.h"

class RedRtpPacket : public RtpPacket
{
public:
	RedRtpPacket();
	RedRtpPacket(RedRtpPacket &src);
	RedRtpPacket(RtpPacket &src);
	~RedRtpPacket();

	void 		SetRed(uint8_t red_payload_type);

private:
	uint8_t		_red_payload_type;
};

