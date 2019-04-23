//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "rtcp_packet.h"

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
    bool padding = (check_data & 0x20) != 0;
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
bool RtcpPacket::RrParseing(int report_count, const std::shared_ptr<const ov::Data> &data)
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
        receiver_report->lsr = stream.ReadBE32(); // NTP timestamp
        receiver_report->dlsr = stream.ReadBE32(); // 1/65536 second

        _receiver_reports.push_back(receiver_report);
    }

    return true;
}

//====================================================================================================
// SR type packet Make
//====================================================================================================
bool RtcpPacket::MakeSrPacket()
{
    return true;
}
