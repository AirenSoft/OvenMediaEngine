#include "rtp_rtcp.h"

RtpRtcp::RtpRtcp(uint32_t id, std::shared_ptr<Session> session)
	: SessionNode(id, SessionNodeType::RtpRtcp, session)
{

}

RtpRtcp::~RtpRtcp()
{
}

bool RtpRtcp::SendOutgoingData(std::shared_ptr<ov::Data> packet)
{
	// Lower Node는 SRTP(DTLS 사용시) 또는 IcePort이다.
	auto node = GetLowerNode();
	if(!node)
	{
		return false;
	}

	//logtd("RtpRtcp Send next node : %d", packet->GetData()->GetLength());
	return node->SendData(GetNodeType(), packet);
}

bool RtpRtcp::SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	// RTPRTCP는 Send를 하는 첫번째 NODE이므로 SendData를 통해 스트림을 받지 않고 SendOutgoingData를 사용한다.
	return true;
}

// Implement SessionNode Interface
// 데이터를 lower(SRTP)에서 받는다.
// Upper Node는 없으므로 자체적으로 처리 완료한다.
bool RtpRtcp::OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	// Node 시작 전에는 아무것도 하지 않는다.
	if(GetState() != SessionNode::NodeState::Started)
	{
		logd("RtpRtcp", "SessionNode has not started, so the received data has been canceled.");
		return false;
	}

	// Publisher에 미디어 데이터를 받는 기능은 없으므로 실질적으로는 RTCP만 들어오게 된다.
	// TODO: RTCP는 여기부터 구현한다.
	return true;
}
