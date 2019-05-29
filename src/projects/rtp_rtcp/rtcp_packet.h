//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once
#include "../base/ovlibrary/ovlibrary.h"

#define RTCP_HEADER_VERSION         (2)
#define RTCP_HEADER_SIZE            (4)
#define RTCP_REPORT_BLOCK_LENGTH    (24)
#define RTCP_MAX_BLOCK_COUNT        (0x1F)

// packet type
enum class RtcpPacketType
{
    SR = 200,   // Sender Report
    RR = 201,   // Receiver Report
    SDES = 202, // Source Description message
    BYE = 203,  // Bye message
    APP = 204,  // Application specfic RTCP
};

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

//====================================================================================================
// RtcpPacket
//====================================================================================================
class RtcpPacket
{
public:
    RtcpPacket();
    virtual ~RtcpPacket();

public :
    std::shared_ptr<ov::Data> GetData(){ return nullptr; }

    static ov::String GetPacketTypeString(RtcpPacketType type);

    static bool IsRtcpPacket(const std::shared_ptr<const ov::Data> &data,
                             RtcpPacketType &packet_type,
                             uint32_t &payload_size,
                             int &report_count);

    static bool RrParseing(int report_count,
                            const std::shared_ptr<const ov::Data> &data,
                            std::vector<std::shared_ptr<RtcpReceiverReport>> &receiver_reports);

    static std::shared_ptr<ov::Data> MakeSrPacket(uint32_t ssrc);

    static double DelayCalculation(uint32_t lsr, uint32_t dlsr);
private :
    static void GetNtpTime(uint32_t &msw, uint32_t &lsw);
};
