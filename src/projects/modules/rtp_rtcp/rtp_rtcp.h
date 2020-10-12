#pragma once

#include "rtp_rtcp_defines.h"
#include "rtp_packetizer.h"
#include "base/publisher/session_node.h"
#include "rtcp_info/rtcp_sr_generator.h"

class RtcSession;
class RtpRtcp : public pub::SessionNode
{
public:
	RtpRtcp(uint32_t id, std::shared_ptr<pub::Session> session, const std::vector<uint32_t> &ssrc_list);
	~RtpRtcp() override;

	// 패킷을 전송한다. 성능을 위해 상위에서 Packetizing을 하는 경우 사용한다.
	bool SendOutgoingData(const std::shared_ptr<RtpPacket> &packet);

	// Implement SessionNode Interface
	// RtpRtcp는 최상위 노드로 SendData를 사용하지 않는다. SendOutgoingData를 사용한다.
	bool SendData(pub::SessionNodeType from_node, const std::shared_ptr<ov::Data> &data) override;
	// Lower Node(SRTP)로부터 데이터를 받는다.
	bool OnDataReceived(pub::SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data) override;
	
private:
    time_t _first_receiver_report_time = 0; // 0 - not received RR packet
    time_t _last_sender_report_time = 0;
    uint64_t _send_packet_sequence_number = 0;

	std::shared_ptr<RtcSession>	_rtc_session;

    std::map<uint32_t, std::shared_ptr<RtcpSRGenerator>> _rtcp_sr_generators;
};
