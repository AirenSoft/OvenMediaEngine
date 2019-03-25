#pragma once

#include "rtp_rtcp_defines.h"
#include "rtp_packetizer.h"
#include <base/publisher/session_node.h>
#include <memory>
#include <vector>

class RtpRtcp : public SessionNode
{
public:
	RtpRtcp(uint32_t id, std::shared_ptr<Session> session);
	~RtpRtcp() override;

	// 패킷을 전송한다. 성능을 위해 상위에서 Packetizing을 하는 경우 사용한다.
	bool SendOutgoingData(std::shared_ptr<ov::Data> packet);

	// Implement SessionNode Interface
	// RtpRtcp는 최상위 노드로 SendData를 사용하지 않는다. SendOutgoingData를 사용한다.
	bool SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data) override;
	// Lower Node(SRTP)로부터 데이터를 받는다.
	bool OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data) override;

private:

};
