#pragma once

#include "rtp_packet.h"

// RTC4588 : RTP Retransmission Payload Format
// https://tools.ietf.org/html/rfc4588

//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                         RTP Header                            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |            OSN                |                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
//    |                  Original RTP Packet Payload                  |
//    |                                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

#define RTX_HEADER_SIZE		2

class RtxRtpPacket : public RtpPacket
{
public:
	RtxRtpPacket(uint32_t rtx_ssrc, uint8_t rtx_payload_type, const RtpPacket &src);
	RtxRtpPacket(const RtxRtpPacket &src);

	uint8_t GetOriginalPayloadType()
	{
		return _origin_payload_type;
	}
	uint16_t GetOriginalSequenceNumber()
	{
		return _origin_seq_no;
	}

	void SetOriginalSequenceNumber(uint16_t seq_no);

private:
	bool PackageAsRtx(uint32_t rtx_ssrc, uint8_t rtx_payload_type, const RtpPacket &src);

	uint8_t		_origin_payload_type; // related(original) payload type
	uint16_t	_origin_seq_no; // original sequence number
};