#include "rtp_rtcp.h"
#include "publishers/webrtc/rtc_application.h"
#include "publishers/webrtc/rtc_stream.h"
#include <base/ovlibrary/byte_io.h>

#define OV_LOG_TAG "RtpRtcp"
#define RTCP_AA_SEND_SEQUENCE (30)

RtpRtcp::RtpRtcp(uint32_t id, std::shared_ptr<pub::Session> session, const std::vector<uint32_t> &ssrc_list)
	        : SessionNode(id, pub::SessionNodeType::Rtp, session)
{
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

bool RtpRtcp::SendOutgoingData(const std::shared_ptr<ov::Data> &packet)
{
	// Lower Node is SRTP
	auto node = GetLowerNode();
	if(!node)
	{
		return false;
	}

    RtpPacket rtp_packet(packet);

    // Parsing error
	if(rtp_packet.Buffer() == nullptr)
    {
        return false;
    }

    if(_rtcp_sr_generators.find(rtp_packet.Ssrc()) == _rtcp_sr_generators.end())
    {
        return false;
    }
    
    auto rtcp_sr_generator = _rtcp_sr_generators[rtp_packet.Ssrc()];
    
    rtcp_sr_generator->AddRTPPacketAndGenerateRtcpSR(rtp_packet);
    if(rtcp_sr_generator->IsAvailableRtcpSRPacket())
    {
        auto rtcp_sr_packet = rtcp_sr_generator->PopRtcpSRPacket();
        if(!node->SendData(pub::SessionNodeType::Rtcp, rtcp_sr_packet))
        {
            logtd("Send RTCP failed : ssrc(%u)", rtp_packet.Ssrc());
        }
		else
		{
			logtd("Send RTCP succeed : ssrc(%u) length(%d)", rtp_packet.Ssrc(), rtcp_sr_packet->GetLength());
		}
    }

	if(!node->SendData(pub::SessionNodeType::Rtp, packet))
    {
		return false;
    }

	return true;
}

bool RtpRtcp::SendData(pub::SessionNodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	// RTPRTCP는 Send를 하는 첫번째 NODE이므로 SendData를 통해 스트림을 받지 않고 SendOutgoingData를 사용한다.
	return true;
}

// Implement SessionNode Interface
// decoded data from srtp
// no upper node( receive data process end)
bool RtpRtcp::OnDataReceived(pub::SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	// nothing to do before node start
	if(GetState() != SessionNode::NodeState::Started)
	{
		logtd("SessionNode has not started, so the received data has been canceled.");
		return false;
	}

    RtcpPacketType packet_type;
    uint32_t payload_size;
    int report_count;

    // rtcp packet check
    if (!RtcpPacket::IsRtcpPacket(data, packet_type, payload_size, report_count) || report_count <= 0)
    {
        logtd("Packet is not RTCP",RtcpPacket::GetPacketTypeString(packet_type).CStr());
        return false;
    }

    return RtcpPacketProcess(packet_type, payload_size, report_count, data);
}

// rtcp packet process
// - current process only RR type
bool RtpRtcp::RtcpPacketProcess(RtcpPacketType packet_type,
                               uint32_t payload_size,
                               int report_count,
                               const std::shared_ptr<const ov::Data> &data)
{
    // Receiver Report
    if (packet_type != RtcpPacketType::RR)
    {
        logtd("RTCP(%s) packet type is not processing",RtcpPacket::GetPacketTypeString(packet_type).CStr());
        return false;
    }

    std::vector<std::shared_ptr<RtcpReceiverReport>> receiver_reports;

    if (!RtcpPacket::RrParseing(report_count, data,  receiver_reports) )
    {
        logtd("RTCP(rr) packet parsing fail", (int) packet_type);
        return false;
    }



    if(_first_receiver_report_time == 0)
    {
        _first_receiver_report_time = receiver_reports[0]->create_time;
    }

    // logtd("RTCP RR(Receiver Report) Packet received - size(%d/%d) report_count(%d)",
    //      payload_size + RTCP_HEADER_SIZE,
    //      data->GetLength(),
    //      report_count);

    for(const auto &receiver_report : receiver_reports)
    {
        // RR info setting
        std::static_pointer_cast<RtcApplication>(GetSession()->GetApplication())->OnReceiverReport(
                GetSession()->GetStream()->GetId(),
            GetSession()->GetId(),
            _first_receiver_report_time,
            receiver_report);
	}
    return true;
}
