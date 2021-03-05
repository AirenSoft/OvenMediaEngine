#include "rtp_rtcp.h"
#include "publishers/webrtc/rtc_application.h"
#include "publishers/webrtc/rtc_stream.h"
#include "rtcp_receiver.h"
#include <base/ovlibrary/byte_io.h>

#define OV_LOG_TAG "RtpRtcp"

RtpRtcp::RtpRtcp(const std::shared_ptr<RtpRtcpInterface> &observer, const std::vector<uint32_t> &ssrc_list)
	        : ov::Node(NodeType::Rtp)
{
	_observer = observer;

    for(auto ssrc : ssrc_list)
    {
        auto rtcp_generator = std::make_shared<RtcpSRGenerator>(ssrc);
        _rtcp_sr_generators[ssrc] = rtcp_generator;
    }
}

RtpRtcp::~RtpRtcp()
{
    _rtcp_sr_generators.clear();
}

bool RtpRtcp::Stop()
{
	// Cross reference
	std::lock_guard<std::shared_mutex> lock(_observer_lock);
	_observer.reset();

	return Node::Stop();
}

bool RtpRtcp::SendOutgoingData(const std::shared_ptr<RtpPacket> &rtp_packet)
{
	// Lower Node is SRTP
	auto node = GetLowerNode();
	if(!node)
	{
		return false;
	}

    if(_rtcp_sr_generators.find(rtp_packet->Ssrc()) != _rtcp_sr_generators.end())
    {
		auto rtcp_sr_generator = _rtcp_sr_generators[rtp_packet->Ssrc()];
		
		rtcp_sr_generator->AddRTPPacketAndGenerateRtcpSR(*rtp_packet);
		if(rtcp_sr_generator->IsAvailableRtcpSRPacket())
		{
			auto rtcp_sr_packet = rtcp_sr_generator->PopRtcpSRPacket();
			if(!node->SendData(NodeType::Rtcp, rtcp_sr_packet->GetData()))
			{
				logd("RTCP","Send RTCP failed : ssrc(%u)", rtp_packet->Ssrc());
			}
			else
			{
				logd("RTCP", "Send RTCP succeed : ssrc(%u) length(%d)", rtp_packet->Ssrc(), rtcp_sr_packet->GetData()->GetLength());
			}
		}
	}

	if(!node->SendData(NodeType::Rtp, rtp_packet->GetData()))
    {
		return false;
    }

	return true;
}

bool RtpRtcp::SendData(NodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	return true;
}

// Implement Node Interface
// decoded data from srtp
// no upper node( receive data process end)
bool RtpRtcp::OnDataReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	// nothing to do before node start
	if(GetState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	// Parse RTCP Packet
	RtcpReceiver receiver;
	if(receiver.ParseCompoundPacket(data) == false)
	{
		return false;
	}

	while(receiver.HasAvailableRtcpInfo())
	{
		auto info = receiver.PopRtcpInfo();
		
		if(_observer != nullptr)
		{
			std::shared_lock<std::shared_mutex> lock(_observer_lock);
			_observer->OnRtcpReceived(info);
		}
	}

    return true;
}
