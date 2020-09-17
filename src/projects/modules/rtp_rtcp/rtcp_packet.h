#pragma once
#include "base/ovlibrary/ovlibrary.h"
#include "rtcp_info/rtcp_info.h"
#include "rtcp_info/receiver_report.h"
#include "rtcp_info/sender_report.h"
#include "rtcp_info/nack.h"

struct RtcpReceiverReport
{
    time_t create_time = time(nullptr);

    uint32_t ssrc = 0;                      // Synchronization source
    uint32_t ssrc_1 = 0;                    //
    uint8_t fraction_lost = 0;              // 1/256
    int32_t packet_lost = 0;                // strema start~current total
    uint16_t sequence_number_cycle = 0;     // sequence number loop cycle count(increase per 65536)
    uint16_t highest_sequence_number = 0;   // the highest sequest number
    uint32_t jitter = 0;                    // jitter
    uint32_t lsr = 0;                       // last SR(NTP timestamp)
    uint32_t dlsr = 0;                      // delay since last SR(1/65536 second)


    double rtt = 0; // (Round Trip Time) calculation form rr packet
};

class RtcpPacket
{
public:
	RtcpPacket(const std::shared_ptr<RtcpInfo> &info);

	std::shared_ptr<RtcpInfo> GetInfo();

	// Return raw data
	std::shared_ptr<ov::Data> GetData(){return nullptr;}

	// Old school
    static std::shared_ptr<ov::Data> MakeSrPacket(uint32_t ssrc, uint32_t rtp_timestamp, uint32_t packet_count, uint32_t octet_count);
    static std::shared_ptr<ov::Data> MakeSrPacket(uint32_t lsr, uint32_t dlsr, uint32_t ssrc, uint32_t rtp_timestamp, uint32_t packet_count, uint32_t octet_count);

private:
	RtcpHeader _header;
	std::shared_ptr<RtcpInfo> _info = nullptr;
};
