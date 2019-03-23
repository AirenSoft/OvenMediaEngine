#pragma once

#include "rtp_packet.h"

class RedRtpPacket : public RtpPacket
{
public:
	RedRtpPacket();
	RedRtpPacket(RedRtpPacket &src);
	RedRtpPacket(RtpPacket &src);
	~RedRtpPacket();

	void 		PackageRed(uint8_t red_payload_type);
	void 		SetMoreBlockBit(bool fBit);

private:
	uint8_t		_red_payload_type;
	bool		_block_bit;
};

