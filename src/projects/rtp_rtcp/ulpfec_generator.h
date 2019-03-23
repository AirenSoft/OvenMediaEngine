/*
 * We referenced this code from WEBRTC NATIVE CODE.
 */

#pragma once

#include "base/common_types.h"
#include "red_rtp_packet.h"

/*
* RTP + RED + FEC
    0                   1                    2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3  4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   | V |P|X|  CC   |M| RED PT(96)  |         SN                    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         timestamp                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         SSRC                                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -----------> RTP Header
   |0| FEC PT(97)  |E|L|P|X|  CC   |M| PT recovery |   SN base     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ -----------> 1 Octet for RED
   |   SN base     |               TS recovery                     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  TS recovery  |        length recovery        |   Protection  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Length      |             mask              |  mask cont.   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                mask cont(only when L=1)       |               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               +
   +                        FEC Payload                            +
   +                                               +---------------+
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

/*
 *	The current version of OME protects all contiguous media packets with one FEC packet,
 *  and one media packet only protects with one FEC packet.
 *  The FEC packet is generated for at least one frame or up to 10 media packets.
 *  When the RTCP implementation is completed in the OME, the number of packets to be protected by one FEC packet
 *  will be determined according to the network state of the session.
 *  We will apply the random mask and the bursty mask model in the near future.
 */

class UlpfecGenerator
{
public:
	UlpfecGenerator();
	~UlpfecGenerator();

	void SetProtectRate(int32_t protection_rate);

	bool AddRtpPacketAndGenerateFec(std::shared_ptr<RedRtpPacket> packet);
	bool IsAvailableFecPackets() const;
	bool NextPacket(RtpPacket *packet);

private:
	bool Encode();
	void XorFecPacket(uint8_t *fec_packet, size_t fec_header_len, RedRtpPacket *packet);
	void FinalizeFecHeader(uint8_t *fec_packet, const size_t fec_payload_len, const uint8_t *mask, const size_t mask_len);

	// 0 ~ 100, 0 means no protect, Network traffic is increased by protect rate.
	// Minimum : 10% (10 media packets / 1 fec packet)
	// 100 means 1 fec packet per 1 media packet.
	int32_t										_new_protection_rate;
	int32_t										_protection_rate;
	bool										_prev_fec_sent;
	std::queue<ov::Data*>						_generated_fec_packets;
	std::vector<std::shared_ptr<RedRtpPacket>>	_media_packets;
};