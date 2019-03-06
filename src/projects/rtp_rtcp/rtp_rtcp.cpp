#include "rtp_rtcp.h"

RtpRtcp::RtpRtcp(uint32_t id, std::shared_ptr<Session> session, bool audio)
	: SessionNode(id, SessionNodeType::RtpRtcp, session)
{
	_audio_session_flag = audio;
}

RtpRtcp::~RtpRtcp()
{
}

void RtpRtcp::Initialize()
{
	_rtp_sender = std::make_unique<RTPSender>(_audio_session_flag, RtpRtcpSession::GetSharedPtr());
	// TODO: _rtcp_sender, _rtcp_receiver 초기화
}

void RtpRtcp::SetPayloadType(uint8_t payload_type)
{
	_rtp_sender->SetPayloadType(payload_type);
}

void RtpRtcp::SetSSRC(const uint32_t ssrc)
{
	_rtp_sender->SetSSRC(ssrc);
}

void RtpRtcp::SetCsrcs(const std::vector<uint32_t> &csrcs)
{
	_rtp_sender->SetCsrcs(csrcs);
}

bool RtpRtcp::SendOutgoingData(std::unique_ptr<RtpPacket> packet)
{
	return SendRtpToNetwork(std::move(packet));
}

bool RtpRtcp::SendOutgoingData(FrameType frame_type,
                               uint32_t time_stamp,
                               const uint8_t *payload_data,
                               size_t payload_size,
                               const FragmentationHeader *fragmentation,
                               const RTPVideoHeader *rtp_video_header)
{


	//TODO: RTCP SENDER REPORT를 전송해야 한다.
//	logd("RTP_RTCP", "RtpRtcp::SendOutgoingData Enter");

	return _rtp_sender->SendOutgoingData(frame_type, time_stamp, payload_data,
	                                     payload_size, fragmentation, rtp_video_header);
}

// RtpSender에서 호출한다.
bool RtpRtcp::SendRtpToNetwork(std::unique_ptr<RtpPacket> packet)
{
	// Lower Node는 SRTP(DTLS 사용시) 또는 IcePort이다.
	auto node = GetLowerNode();
	if(!node)
	{
		return false;
	}

	//logtd("RtpRtcp Send next node : %d", packet->GetData()->GetLength());
	return node->SendData(GetNodeType(), packet->GetData());
}

// RtcpSender에서 호출한다.
bool RtpRtcp::SendRtcpToNetwork(std::unique_ptr<RtcpPacket> packet)
{
	return true;
}

bool RtpRtcp::SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	// RTPRTCP는 Send를 하는 첫번 NODE로 SendData를 통해 스트림을 받지 않고 SendOutgoingData를 사용한다.
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

	// 미디어 데이터를 받는 기능은 현재 없으므로 실질적으로는 RTCP만 들어오게 된다.
	// TODO: RTCP는 여기부터 구현한다.
	return true;
}
