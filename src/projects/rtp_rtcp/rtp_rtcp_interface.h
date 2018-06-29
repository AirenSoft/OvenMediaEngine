#pragma once

#include "rtp_packet.h"
#include "rtcp_packet.h"

// 상위 클래스는 RtpRtcp에 Session(Transport)과 관련된 모든 정보와 기능을 제공한다.
class RtpRtcpSession : public ov::EnableSharedFromThis<RtpRtcpSession>
{
public:
    // RTP Packet을 전송한다.
    virtual bool        SendRtpToNetwork(std::unique_ptr<RtpPacket> packet) = 0;
    // RTCP Packet을 전송한다.
    virtual bool        SendRtcpToNetwork(std::unique_ptr<RtcpPacket> packet) = 0;
};
