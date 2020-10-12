#pragma once

#include "rtp_packet.h"
#include "rtcp_packet.h"

class RtpRtcpPacketizerInterface : public ov::EnableSharedFromThis<RtpRtcpPacketizerInterface>
{
public:
    // RTP Packet을 전송한다.
    virtual bool        OnRtpPacketized(std::shared_ptr<RtpPacket> packet) = 0;
};
