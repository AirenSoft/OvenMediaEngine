#pragma once

#include "rtp_rtcp_defines.h"
#include "rtp_packetizer.h"
#include "../base/publisher/session_node.h"
#include <memory>
#include <vector>
#include <map>
#include "rtcp_packet.h"

struct RtcpInfo
{
    RtcpInfo(uint32_t ssrc_)
    {
        ssrc = ssrc_;
        sequence_number = 0;
    }

    uint32_t ssrc = 0;
    uint32_t sequence_number = 0;
    // uint32_t rtp_packet_size = 0;
    // time_t rtp_packt_send_time = time(nullptr);
    // uint32_t rtp_packt_timestamp = 0;
 };

class RtpRtcp : public SessionNode
{
public:
	RtpRtcp(uint32_t id, std::shared_ptr<Session> session, const std::vector<uint32_t> &ssrc_list);

	~RtpRtcp() override;

	// 패킷을 전송한다. 성능을 위해 상위에서 Packetizing을 하는 경우 사용한다.
	bool SendOutgoingData(std::shared_ptr<ov::Data> packet);

	// Implement SessionNode Interface
	// RtpRtcp는 최상위 노드로 SendData를 사용하지 않는다. SendOutgoingData를 사용한다.
	bool SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data) override;
	// Lower Node(SRTP)로부터 데이터를 받는다.
	bool OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data) override;

	bool RtcpPacketProcess(RtcpPacketType packet_type,
                            uint32_t payload_size,
                            int report_count,
                            const std::shared_ptr<const ov::Data> &data);
private:
    time_t _first_receiver_report_time = 0; // 0 - not received RR packet

    time_t _last_sender_report_time = 0;
    uint64_t _send_packet_sequence_number = 0;
    std::vector<std::shared_ptr<RtcpInfo>> _rtcp_infos;
};
