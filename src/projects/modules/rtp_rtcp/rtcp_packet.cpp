//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "rtcp_packet.h"
#include "rtcp_info/rtcp_private.h"
#include <base/ovlibrary/byte_io.h>
#include <sys/time.h>

RtcpPacket::RtcpPacket(const std::shared_ptr<RtcpInfo> &info)
{

}

/////////////////////////////
// OLD SCHOOL
/////////////////////////////
#define DEFAULT_MAX_PACKET_SIZE		1472
#define DEFAULT_SR_LENGTH           6 // byte/4

std::shared_ptr<ov::Data> RtcpPacket::MakeSrPacket(uint32_t msw, uint32_t lsw, uint32_t ssrc, uint32_t rtp_timestamp, uint32_t packet_count, uint32_t octet_count)
{
    // Bigger size is needed for SRTCP, It is temporary codes. 
    auto sr_packet = std::make_shared<ov::Data>(50);

    sr_packet->SetLength(28);
    auto buffer = sr_packet->GetWritableDataAs<uint8_t>();

    buffer[0] = RTCP_HEADER_VERSION << 6;
    buffer[1] = static_cast<uint8_t>(RtcpPacketType::SR);
    ByteWriter<uint16_t>::WriteBigEndian(&buffer[2], DEFAULT_SR_LENGTH);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[4], ssrc);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[8], msw);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[12], lsw);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[16], rtp_timestamp);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[20], packet_count);
    ByteWriter<uint32_t>::WriteBigEndian(&buffer[24], octet_count);

    return sr_packet;
}

std::shared_ptr<ov::Data> RtcpPacket::MakeSrPacket(uint32_t ssrc, uint32_t rtp_timestamp, uint32_t packet_count, uint32_t octet_count)
{
    uint32_t msw = 0;
    uint32_t lsw = 0;

    ov::Clock::GetNtpTime(msw, lsw);
    return MakeSrPacket(msw, lsw, ssrc, rtp_timestamp, packet_count, octet_count);
}
