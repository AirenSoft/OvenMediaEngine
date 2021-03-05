#pragma once

#include "rtp_rtcp_defines.h"
#include "rtp_packetizer.h"
#include "base/ovlibrary/node.h"
#include "rtcp_info/rtcp_sr_generator.h"

class RtcSession;

class RtpRtcpInterface : public ov::EnableSharedFromThis<RtpRtcpInterface>
{
public:
	virtual void OnRtpReceived(const std::shared_ptr<RtpPacket> &rtp_packet) = 0;
	virtual void OnRtcpReceived(const std::shared_ptr<RtcpInfo> &rtcp_info) = 0;
};

class RtpRtcp : public ov::Node
{
public:
	RtpRtcp(const std::shared_ptr<RtpRtcpInterface> &observer, const std::vector<uint32_t> &ssrc_list);
	~RtpRtcp() override;

	bool Stop() override;

	bool SendOutgoingData(const std::shared_ptr<RtpPacket> &packet);

	// Implement Node Interface
	bool SendData(NodeType from_node, const std::shared_ptr<ov::Data> &data) override;
	bool OnDataReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data) override;
	
private:
    time_t _first_receiver_report_time = 0; // 0 - not received RR packet
    time_t _last_sender_report_time = 0;
    uint64_t _send_packet_sequence_number = 0;

	std::shared_mutex _observer_lock;
	std::shared_ptr<RtpRtcpInterface> _observer;

    std::map<uint32_t, std::shared_ptr<RtcpSRGenerator>> _rtcp_sr_generators;
};
