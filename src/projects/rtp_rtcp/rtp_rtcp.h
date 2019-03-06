#pragma once

#include "rtp_rtcp_defines.h"
#include "rtp_sender.h"
#include <base/publisher/session_node.h>
#include <memory>
#include <vector>

class RtpRtcp : public SessionNode, public RtpRtcpSession
{
public:
	RtpRtcp(uint32_t id, std::shared_ptr<Session> session, bool audio);
	~RtpRtcp() override;

	void Initialize();

	// Payload Type을 설정한다.
	void SetPayloadType(uint8_t payload_type);
	// SSRC를 설정한다.
	void SetSSRC(uint32_t ssrc);
	// CSRC를 설정한다.
	void SetCsrcs(const std::vector<uint32_t> &csrcs);

	// 이미 만들어진 패킷을 전송한다. 성능을 위해 상위에서 Packetizing을 하는 경우 사용한다.
	bool SendOutgoingData(std::unique_ptr<RtpPacket> packet);

	// Frame을 전송한다.
	bool SendOutgoingData(FrameType frame_type,
	                      uint32_t time_stamp,
	                      const uint8_t *payload_data,
	                      size_t payload_size,
	                      const FragmentationHeader *fragmentation,
	                      const RTPVideoHeader *rtp_video_header);

	// RtpRtcpSession Implementation
	// RtpSender, RtcpSender 등에 RtpRtcpSession을 넘겨서 전송 이 함수를 통해 하도록 한다.
	bool SendRtpToNetwork(std::unique_ptr<RtpPacket> packet) override;
	bool SendRtcpToNetwork(std::unique_ptr<RtcpPacket> packet) override;

	// Implement SessionNode Interface
	// RtpRtcp는 최상위 노드로 SendData를 사용하지 않는다. SendOutgoingData를 사용한다.
	bool SendData(SessionNodeType from_node, const std::shared_ptr<ov::Data> &data) override;
	// Lower Node(SRTP)로부터 데이터를 받는다.
	bool OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data) override;


private:
	bool _audio_session_flag;
	std::unique_ptr<RTPSender> _rtp_sender;
	//TODO: RTCP는 향후 구현
	//RTCPSender				_rtcp_sender;
	//RTCPReceiver				_rtcp_receiver;
};
