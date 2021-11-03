#include <base/ovlibrary/byte_io.h>

#include "rtp_rtcp.h"
#include "publishers/webrtc/rtc_application.h"
#include "publishers/webrtc/rtc_stream.h"
#include "rtcp_receiver.h"
#include "rtcp_info/fir.h"
#include "rtcp_info/pli.h"

#include "modules/rtsp/rtsp_data.h"

#define OV_LOG_TAG "RtpRtcp"

RtpRtcp::RtpRtcp(const std::shared_ptr<RtpRtcpInterface> &observer)
	        : ov::Node(NodeType::Rtp)
{
	_observer = observer;
	_receiver_report_timer.Start();
	_rtcp_send_stop_watch.Start();
}

RtpRtcp::~RtpRtcp()
{
    _rtcp_sr_generators.clear();
}

bool RtpRtcp::AddRtpSender(uint8_t payload_type, uint32_t ssrc, uint32_t codec_rate, ov::String cname)
{
	std::shared_lock<std::shared_mutex> lock(_state_lock);
	if(GetNodeState() != ov::Node::NodeState::Ready)
	{
		logtd("It can only be called in the ready state.");
		return false;
	}

	_rtcp_sr_generators[ssrc] = std::make_shared<RtcpSRGenerator>(ssrc, codec_rate);

	if(_sdes == nullptr)
	{
		_sdes = std::make_shared<Sdes>();
	}

	_sdes->AddChunk(std::make_shared<SdesChunk>(ssrc, SdesChunk::Type::CNAME, cname));

	logtd("AddRtpSender : %d / %u / %u / %s", payload_type, ssrc, codec_rate, cname.CStr());

	return true;
}

bool RtpRtcp::AddRtpReceiver(uint32_t track_id, const std::shared_ptr<MediaTrack> &track)
{
	std::shared_lock<std::shared_mutex> lock(_state_lock);
	if(GetNodeState() != ov::Node::NodeState::Ready)
	{
		logtd("It can only be called in the ready state.");
		return false;
	}

	_tracks[track_id] = track;

	switch(track->GetOriginBitstream())
	{
		case cmn::BitstreamFormat::H264_RTP_RFC_6184:
		case cmn::BitstreamFormat::VP8_RTP_RFC_7741:
		case cmn::BitstreamFormat::AAC_MPEG4_GENERIC:
			_rtp_frame_jitter_buffers[track_id] = std::make_shared<RtpFrameJitterBuffer>();
			break;
		case cmn::BitstreamFormat::OPUS_RTP_RFC_7587:
			_rtp_minimal_jitter_buffers[track_id] = std::make_shared<RtpMinimalJitterBuffer>();
			break;
		default:
			logte("RTP Receiver cannot support %d input stream format", static_cast<int8_t>(track->GetOriginBitstream()));
			return false;
	}

	return true;
}

bool RtpRtcp::Stop()
{
	// Cross reference
	std::lock_guard<std::shared_mutex> lock(_state_lock);
	_observer.reset();

	return Node::Stop();
}

bool RtpRtcp::SendRtpPacket(const std::shared_ptr<RtpPacket> &rtp_packet)
{
	std::shared_lock<std::shared_mutex> lock(_state_lock);
	// nothing to do before node start
	if(GetNodeState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	// RTCP(SR + SR + SDES + SDES)
	auto it = _rtcp_sr_generators.find(rtp_packet->Ssrc());
    if(it != _rtcp_sr_generators.end())
    {
		auto rtcp_sr_generator = it->second;
		rtcp_sr_generator->AddRTPPacketAndGenerateRtcpSR(*rtp_packet);
	}

	if(_rtcp_sent_count == 0 || _rtcp_send_stop_watch.Elapsed() > SDES_CYCLE_MS)
	{		
		_rtcp_send_stop_watch.Update();
		_rtcp_sent_count ++;

		auto compound_rtcp_data = std::make_shared<ov::Data>(1024);
		for(const auto &item : _rtcp_sr_generators)
		{
			auto rtcp_sr_generator = item.second;
			auto rtcp_sr_packet = rtcp_sr_generator->PopRtcpSRPacket();
			if(rtcp_sr_packet == nullptr)
			{
				continue;
			}
			compound_rtcp_data->Append(rtcp_sr_packet->GetData());
		}

		if(_rtcp_sdes == nullptr)
		{
			_rtcp_send_stop_watch.Update();
			_rtcp_sdes = std::make_shared<RtcpPacket>();
			_rtcp_sdes->Build(_sdes);
		}

		compound_rtcp_data->Append(_rtcp_sdes->GetData());
		
		if(SendDataToNextNode(NodeType::Rtcp, compound_rtcp_data) == false)
		{
			logd("RTCP","Send RTCP failed : pt(%d) ssrc(%u)", rtp_packet->PayloadType(), rtp_packet->Ssrc());
		}
		else
		{
			logd("RTCP", "Send RTCP succeed : pt(%d) ssrc(%u) length(%d)", rtp_packet->PayloadType(), rtp_packet->Ssrc(), compound_rtcp_data->GetLength());
		}
	}

	// Send RTP
	_last_sent_rtp_packet = rtp_packet;
	return SendDataToNextNode(NodeType::Rtp, rtp_packet->GetData());
}

bool RtpRtcp::SendPLI(uint32_t media_ssrc)
{
	auto stat_it = _receive_statistics.find(media_ssrc);
	if(stat_it == _receive_statistics.end())
	{
		// Never received such SSRC packet
		return false;
	}

	auto stat = stat_it->second;
	
	auto pli = std::make_shared<PLI>();

	pli->SetSrcSsrc(stat->GetReceiverSSRC());
	pli->SetMediaSsrc(media_ssrc);

	auto rtcp_packet = std::make_shared<RtcpPacket>();
	rtcp_packet->Build(pli);

	_last_sent_rtcp_packet = rtcp_packet;

	return SendDataToNextNode(NodeType::Rtcp, rtcp_packet->GetData());
}

bool RtpRtcp::SendFIR(uint32_t media_ssrc)
{
	auto stat_it = _receive_statistics.find(media_ssrc);
	if(stat_it == _receive_statistics.end())
	{
		// Never received such SSRC packet
		return false;
	}

	auto stat = stat_it->second;
	
	auto fir = std::make_shared<FIR>();

	fir->SetSrcSsrc(stat->GetReceiverSSRC());
	fir->SetMediaSsrc(media_ssrc);
	fir->AddFirMessage(media_ssrc, static_cast<uint8_t>(stat->GetNumberOfFirRequests()%256));
	auto rtcp_packet = std::make_shared<RtcpPacket>();
	rtcp_packet->Build(fir);

	stat->OnFirRequested();

	_last_sent_rtcp_packet = rtcp_packet;

	return SendDataToNextNode(NodeType::Rtcp, rtcp_packet->GetData());
}

uint8_t RtpRtcp::GetReceivedPayloadType(uint32_t ssrc)
{
	auto stat_it = _receive_statistics.find(ssrc);
	if(stat_it == _receive_statistics.end())
	{
		return 0;
	}

	return stat_it->second->GetPayloadType();
}

// In general, since RTP_RTCP is the first node, there is no previous node. So it will not be called
bool RtpRtcp::OnDataReceivedFromPrevNode(NodeType from_node, const std::shared_ptr<ov::Data> &data)
{
	std::shared_lock<std::shared_mutex> lock(_state_lock);
	// nothing to do before node start
	if(GetNodeState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	if(SendDataToNextNode(from_node, data) == false)
	{
		loge("RtpRtcp","Send data failed from(%d) data_len(%d)", static_cast<uint16_t>(from_node), data->GetLength());
		return false;
	}

	return true;
}

// Implement Node Interface
// decoded data from srtp
// no upper node( receive data process end)
bool RtpRtcp::OnDataReceivedFromNextNode(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	// In the case of UDP, one complete packet is received here.
	// In the case of TCP, demuxing is already performed in the lower layer 
	// such as IcePort or RTSP Interleaved channel to complete and input one packet.
	// Therefore, it is not necessary to demux the packet here.

	std::shared_lock<std::shared_mutex> lock(_state_lock);
	// nothing to do before node start
	if(GetNodeState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	// std::min(FIXED_HEADER_SIZE, RTCP_HEADER_SIZE)
	if(data->GetLength() < RTCP_HEADER_SIZE)
	{
		logtd("It is not an RTP or RTCP packet.");
		return false;
	}

	/* Check if this is a RTP/RTCP packet
		https://www.rfc-editor.org/rfc/rfc7983.html
					+----------------+
					|        [0..3] -+--> forward to STUN
					|                |
					|      [16..19] -+--> forward to ZRTP
					|                |
		packet -->  |      [20..63] -+--> forward to DTLS
					|                |
					|      [64..79] -+--> forward to TURN Channel
					|                |
					|    [128..191] -+--> forward to RTP/RTCP
					+----------------+
	*/
	auto first_byte = data->GetDataAs<uint8_t>()[0];
	if(first_byte >= 128 && first_byte <= 191)
	{
		// Distinguish between RTP and RTCP 
		// https://tools.ietf.org/html/rfc5761#section-4
		auto payload_type = data->GetDataAs<uint8_t>()[1];
		// RTCP
		if(payload_type >= 192 && payload_type <= 223)
		{
			return OnRtcpReceived(from_node, data);
		}
		// RTP
		else
		{
			return OnRtpReceived(from_node, data);
		}
	}
	else
	{
		logtd("It is not an RTP or RTCP packet.");
		return false;
	}

    return true;
}

bool RtpRtcp::OnRtpReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	auto packet = std::make_shared<RtpPacket>(data);
	logtd("%s", packet->Dump().CStr());

	uint32_t track_id = 0;
	if(from_node == NodeType::Rtsp)
	{
		auto rtsp_data = std::dynamic_pointer_cast<const RtspData>(data);
		if(rtsp_data == nullptr)
		{
			logte("Could not convert to RtspData");
			return false;
		}

		// RTSP Node uses channelID as trackID
		track_id = rtsp_data->GetChannelId();
		packet->SetRtspChannel(track_id);
	}
	else
	{
		track_id = packet->Ssrc();
	}

	auto track_it = _tracks.find(track_id);
	if(track_it == _tracks.end())
	{
		logte("Could not find track info for track ID %u", track_id);
		return false;
	}
	auto track = track_it->second;

	std::shared_ptr<RtpReceiveStatistics> stat;
	auto stat_it = _receive_statistics.find(packet->Ssrc());
	if(stat_it == _receive_statistics.end())
	{
		// First receive
		stat = std::make_shared<RtpReceiveStatistics>(packet->PayloadType(), packet->Ssrc(), track->GetTimeBase().GetDen());
		_receive_statistics.emplace(packet->Ssrc(), stat);
	}
	else
	{
		stat = stat_it->second;
	}

	stat->AddReceivedRtpPacket(packet);

	// Send ReceiverReport
	if(stat->HasElapsedSinceLastReportBlock(RECEIVER_REPORT_CYCLE_MS))
	{
		auto report = std::make_shared<ReceiverReport>();
		report->SetRtpSsrc(packet->Ssrc());
		report->SetSenderSsrc(stat->GetReceiverSSRC());
		report->AddReportBlock(stat->GenerateReportBlock());

		auto rtcp_packet = std::make_shared<RtcpPacket>();
		if(rtcp_packet->Build(report) == true)
		{
			_last_sent_rtcp_packet = rtcp_packet;
			SendDataToNextNode(NodeType::Rtcp, rtcp_packet->GetData());
		}
	}

	int jitter_buffer_type = 0;
	switch(track->GetOriginBitstream())
	{
		case cmn::BitstreamFormat::H264_RTP_RFC_6184:
		case cmn::BitstreamFormat::VP8_RTP_RFC_7741:
		case cmn::BitstreamFormat::AAC_MPEG4_GENERIC:
			jitter_buffer_type = 1;
			break;
		case cmn::BitstreamFormat::OPUS_RTP_RFC_7587:
			jitter_buffer_type = 2;
			break;
		default:
			break;
	}

	if(jitter_buffer_type == 1)
	{
		auto buffer_it = _rtp_frame_jitter_buffers.find(track_id);
		if(buffer_it == _rtp_frame_jitter_buffers.end())
		{
			// can not happen
			logte("Could not find jitter buffer for payload type %d", packet->PayloadType());
			return false;
		}

		auto jitter_buffer = buffer_it->second;

		jitter_buffer->InsertPacket(packet);

		auto frame = jitter_buffer->PopAvailableFrame();
		if(frame != nullptr && _observer != nullptr)
		{
			std::vector<std::shared_ptr<RtpPacket>> rtp_packets;

			auto packet = frame->GetFirstRtpPacket();
			if(packet == nullptr)
			{
				// can not happen
				logte("Could not get first rtp packet from jitter buffer - ssrc : %u", packet->Ssrc());
				return false;
			}

			rtp_packets.push_back(packet);

			while(true)
			{
				packet = frame->GetNextRtpPacket();
				if(packet == nullptr)
				{
					break;
				}

				rtp_packets.push_back(packet);
			}

			_observer->OnRtpFrameReceived(rtp_packets);
		}
	}
	else if(jitter_buffer_type == 2)
	{
		auto buffer_it = _rtp_minimal_jitter_buffers.find(track_id);
		if(buffer_it == _rtp_minimal_jitter_buffers.end())
		{
			// can not happen
			logte("Could not find jitter buffer for ssrc %u", packet->Ssrc());
			return false;
		}

		auto jitter_buffer = buffer_it->second;

		jitter_buffer->InsertPacket(packet);

		auto pop_packet = jitter_buffer->PopAvailablePacket();
		if(pop_packet != nullptr)
		{
			std::vector<std::shared_ptr<RtpPacket>> rtp_packets;
			rtp_packets.push_back(pop_packet);
			_observer->OnRtpFrameReceived(rtp_packets);
		}
	}
	else
	{
		logte("Could not find jitter buffer for payload type %d", packet->PayloadType());
	}

	return true;
}

bool RtpRtcp::OnRtcpReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	logtd("Get RTCP Packet - length(%d)", data->GetLength());
	// Parse RTCP Packet
	RtcpReceiver receiver;
	if(receiver.ParseCompoundPacket(data) == false)
	{
		return false;
	}

	uint32_t rtsp_channel = 0;
	if(from_node == NodeType::Rtsp)
	{
		auto rtsp_data = std::dynamic_pointer_cast<const RtspData>(data);
		if(rtsp_data == nullptr)
		{
			logte("Could not convert to RtspData");
			return false;
		}

		// RTSP Node uses channelID as trackID
		rtsp_channel = rtsp_data->GetChannelId();
	}

	while(receiver.HasAvailableRtcpInfo())
	{
		auto info = receiver.PopRtcpInfo();
		info->SetRtspChannel(rtsp_channel);

		if(info->GetPacketType() == RtcpPacketType::SR)
		{
			auto sr = std::dynamic_pointer_cast<SenderReport>(info);
			auto stat_it = _receive_statistics.find(sr->GetSenderSsrc());
			if(stat_it != _receive_statistics.end())
			{
				auto stat = stat_it->second;
				stat->AddReceivedRtcpSenderReport(sr);
			}
		}
		
		if(_observer != nullptr)
		{
			_observer->OnRtcpReceived(info);
		}
	}
	
	return true;
}

std::shared_ptr<RtpPacket> RtpRtcp::GetLastSentRtpPacket()
{
	return _last_sent_rtp_packet;
}

std::shared_ptr<RtcpPacket> RtpRtcp::GetLastSentRtcpPacket()
{
	return _last_sent_rtcp_packet;
}