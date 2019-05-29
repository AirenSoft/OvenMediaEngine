//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "rtcp_packet.h"
#include <sys/time.h>

#define OV_LOG_TAG "Rtcp"

/*
 **********  RR: Receiver report RTCP packet **********
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|    RC   |   PT=RR=201   |             length            | header
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     SSRC of packet sender                     |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 SSRC_1 (SSRC of first source)                 | report
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
| fraction lost |       cumulative number of packets lost       |   1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           extended highest sequence number received           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      interarrival jitter                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         last SR (LSR)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   delay since last SR (DLSR)                  |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                 SSRC_2 (SSRC of second source)                | report
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
:                               ...                             :   2
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|                  profile-specific extensions                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


Receiver Estimated Max Bitrate (REMB)
0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P| FMT=15  |   PT=206      |             length            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  SSRC of packet sender                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  SSRC of media source                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Unique identifier 'R' 'E' 'M' 'B'                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Num SSRC     | BR Exp    |  BR Mantissa                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   SSRC feedback                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  ...                                                          |


 jitter
 - Si is the RTP timestamp from packet i
 - Ri is the time of arrival in RTP timestamp units for packet i
 - D(i,j)=(Rj-Ri)-(Sj-Si)=(Rj-Sj)-(Ri-Si)
 - J=J+(|D(i-1,i)|-J)/16

*/


RtcpPacket::RtcpPacket()
{

}

RtcpPacket::~RtcpPacket()
{
}

//====================================================================================================
// Rtcp Packet Vaild Check
//====================================================================================================
ov::String  RtcpPacket::GetPacketTypeString(RtcpPacketType type)
{
    ov::String result = "unknown";

    switch(type)
    {
        case RtcpPacketType::SR :
            result = "SR";
            break;
        case RtcpPacketType::RR :
            result = "RR";
            break;
        case RtcpPacketType::SDES :
            result = "SDES";
            break;
        case RtcpPacketType::BYE :
            result = "BYE";
            break;
        case RtcpPacketType::APP :
            result = "APP";
            break;
    }

    return result;
}

//====================================================================================================
// Rtcp Packet Vaild Check
//====================================================================================================
bool RtcpPacket::IsRtcpPacket(const std::shared_ptr<const ov::Data> &data,
                               RtcpPacketType &packet_type,
                               uint32_t &payload_size,
                               int &report_count)
{
    if(data->GetLength() < RTCP_HEADER_SIZE)
        return false;

    ov::ByteStream stream(data.get());


    const uint8_t check_data =  stream.Read8();
    int version = (check_data >> 6);

    report_count = (check_data & RTCP_MAX_BLOCK_COUNT);
    int type = stream.Read8();
    payload_size = stream.ReadBE16()*4;

    // rtcp rr packet check
    if(version != RTCP_HEADER_VERSION ||
       type < (int)RtcpPacketType::SR ||
       type > (int)RtcpPacketType::APP ||
       data->GetLength() < RTCP_HEADER_SIZE + payload_size)
    {
        return false;
    }

    packet_type = (RtcpPacketType)type;

    return true;
}

//====================================================================================================
// RR type packet Parsing
//====================================================================================================
bool RtcpPacket::RrParseing(int report_count,
                            const std::shared_ptr<const ov::Data> &data,
                            std::vector<std::shared_ptr<RtcpReceiverReport>> &receiver_reports)
{
    ov::ByteStream stream(data.get());
    stream.Skip(RTCP_HEADER_SIZE);
    uint32_t sender_ssrc = stream.ReadBE32();

    for(int index = 0; index < report_count; index++)
    {
        auto receiver_report = std::make_shared<RtcpReceiverReport >();
        receiver_report->ssrc = sender_ssrc;
        receiver_report->ssrc_1 = stream.ReadBE32();
        receiver_report->fraction_lost = stream.Read8();

        receiver_report->packet_lost += stream.Read8() << 16;
        receiver_report->packet_lost += stream.Read8() << 8;
        receiver_report->packet_lost += stream.Read8();

        receiver_report->sequence_number_cycle = stream.ReadBE16();
        receiver_report->highest_sequence_number = stream.ReadBE16();
        receiver_report->jitter = stream.ReadBE32();
        receiver_report->lsr = stream.ReadBE32();
        receiver_report->dlsr = stream.ReadBE32();

        // delay calculation
        receiver_report->rtt = DelayCalculation(receiver_report->lsr, receiver_report->dlsr);

        receiver_reports.push_back(receiver_report);
    }

    return true;
}

//====================================================================================================
// SR type packet Make
/*
 *  **********  SR: Sender report RTCP packet **********
         0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    RC   |   PT=SR=200   |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         SSRC of sender                        |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
sender |              NTP timestamp, most significant word             |
info   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |             NTP timestamp, least significant word             |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         RTP timestamp                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                     sender's packet count                     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      sender's octet count                     |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
report |                 SSRC_1 (SSRC of first source)                 |
block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  1    | fraction lost |       cumulative number of packets lost       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |           extended highest sequence number received           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      interarrival jitter                      |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         last SR (LSR)                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                   delay since last SR (DLSR)                  |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
report |                 SSRC_2 (SSRC of second source)                |
block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  2    :                               ...                             :
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
       |                  profile-specific extensions                  |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
//====================================================================================================
#define DEFAULT_MAX_PACKET_SIZE		1472
#define DEFAULT_SR_LENGTH 6 // byte/4

std::shared_ptr<ov::Data> RtcpPacket::MakeSrPacket(uint32_t ssrc)
{
    uint32_t msw = 0;
    uint32_t lsw = 0;

    auto sr_packet = std::make_shared<ov::Data>(DEFAULT_MAX_PACKET_SIZE);

    ov::ByteStream stream(sr_packet.get());

    GetNtpTime(msw, lsw);

    uint8_t data = 0;
    data += (RTCP_HEADER_VERSION) << 6; // version;
    data += (0&RTCP_MAX_BLOCK_COUNT); // RC;
    stream.Write8(data);

    // payload type SR
    stream.Write8(static_cast<uint8_t>(RtcpPacketType::SR));

    // length
    stream.WriteBE16(DEFAULT_SR_LENGTH); // SSRC(4) NTP(8) tm(4) pc(4) oc(4)

    // SSRC of sender
    stream.WriteBE32(ssrc);

    // NTP timestamp, most significant word
    stream.WriteBE32(msw);

    // NTP timestamp, least significant word
    stream.WriteBE32(lsw);

    // RTP timestamp
    stream.WriteBE32(0);

    // sender's packet count
    stream.WriteBE32(0);

    // sender's octet count
    stream.WriteBE32(0);

    return sr_packet;
}

#define GETTIMEOFDAY_TO_NTP_OFFSET 2208988800 //  Number of seconds between 1-Jan-1900 and 1-Jan-1970

void RtcpPacket::GetNtpTime(uint32_t &msw, uint32_t &lsw)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

//    struct timeval now;
//    gettimeofday(&now, nullptr); // get current time

    msw = (uint32_t)(now.tv_sec) + GETTIMEOFDAY_TO_NTP_OFFSET;
    lsw = (uint32_t)((double)(now.tv_nsec/1000)*(double)(((uint64_t)1)<<32)*1.0e-6);
}

//====================================================================================================
// Delay Calculation
// - calculation form rr packet
// - 0 is pass
//====================================================================================================
double RtcpPacket::DelayCalculation(uint32_t lsr, uint32_t dlsr)
{
    if(lsr == 0 || dlsr == 0)
        return 0;

    uint32_t msw = 0;
    uint32_t lsw = 0;

    GetNtpTime(msw, lsw);

    uint32_t tr = ((msw & 0xFFFF) << 16) | (lsw >> 16);

    if((tr - lsr) <= dlsr)
        return 0;

    return static_cast<double>(tr - lsr - dlsr)/65536.0;
}
