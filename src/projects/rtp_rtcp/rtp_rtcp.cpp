#include "rtp_rtcp.h"
#include "../webrtc/rtc_application.h"
#include "../webrtc/rtc_stream.h"
#include <base/ovlibrary/byte_io.h>

#define OV_LOG_TAG "RtpRtcp"
#define RTCP_AA_SEND_SEQUENCE (1000)

RtpRtcp::RtpRtcp(uint32_t id, std::shared_ptr<Session> session, const std::vector<uint32_t> &ssrc_list)
	        : SessionNode(id, SessionNodeType::RtpRtcp, session)
{
    for(auto ssrc : ssrc_list)
    {
        auto rtcp_info = std::make_shared<RtcpInfo>(ssrc);
        _rtcp_infos.push_back(rtcp_info);
    }
}

RtpRtcp::~RtpRtcp()
{
}

bool RtpRtcp::SendOutgoingData(std::shared_ptr<ov::Data> packet)
{
	// Lower Node is SRTP
	auto node = GetLowerNode();

	if(!node)
	{
		return false;
	}

	if(_first_receiver_report_time != 0)
    {
        auto byte_buffer = packet->GetDataAs<uint8_t>();
        uint32_t ssrc = ByteReader<uint32_t>::ReadBigEndian(&byte_buffer[8]);

        // rtcp aa packet send ( per 1000)
        for(auto &rtcp_info : _rtcp_infos)
        {
            if(rtcp_info->ssrc == ssrc)
            {
                if (rtcp_info->sequence_number == 0)
                {
                    logtd("Send rtcp sr packet - ssrc(%u)", ssrc);

                    if (!dynamic_cast<SrtpTransport *>(node.get())->SendRtcpData(GetNodeType(),
                            RtcpPacket::MakeSrPacket(ssrc)))
                    {
                        logtw("Rtcp sr packet send fail - ssrc(%u)", ssrc);
                    }
                }

                rtcp_info->sequence_number++;

                if(rtcp_info->sequence_number >= RTCP_AA_SEND_SEQUENCE)
                    rtcp_info->sequence_number = 0;

                break;
            }
        }
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
// decoded data from srtp
// no upper node( receive data process end)
bool RtpRtcp::OnDataReceived(SessionNodeType from_node, const std::shared_ptr<const ov::Data> &data)
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
