#include "rtp_rtcp.h"
#include "publishers/webrtc/rtc_application.h"
#include "publishers/webrtc/rtc_stream.h"
#include "rtcp_receiver.h"
#include <base/ovlibrary/byte_io.h>

#define OV_LOG_TAG "RtpRtcp"

RtpRtcp::RtpRtcp(const std::shared_ptr<RtpRtcpInterface> &observer)
	        : ov::Node(NodeType::Rtp)
{
	_observer = observer;
}

RtpRtcp::~RtpRtcp()
{
    _rtcp_sr_generators.clear();
}

bool RtpRtcp::AddRtcpSRGenerator(uint8_t payload_type, uint32_t ssrc)
{
	std::shared_lock<std::shared_mutex> lock(_state_lock);
	if(GetState() != ov::Node::NodeState::Ready)
	{
		logtd("It can only be called in the ready state.");
		return false;
	}

	_rtcp_sr_generators[payload_type] = std::make_shared<RtcpSRGenerator>(ssrc);
	return true;
}

bool RtpRtcp::AddRtpReceiver(uint8_t payload_type, const std::shared_ptr<MediaTrack> &track)
{
	std::shared_lock<std::shared_mutex> lock(_state_lock);
	if(GetState() != ov::Node::NodeState::Ready)
	{
		logtd("It can only be called in the ready state.");
		return false;
	}

	_tracks[payload_type] = track;

	// Only use JitterBuffer for Video
	if(track->GetMediaType() == cmn::MediaType::Video)
	{
		_rtp_jitter_buffers[payload_type] = std::make_shared<RtpJitterBuffer>();
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

bool RtpRtcp::SendOutgoingData(const std::shared_ptr<RtpPacket> &rtp_packet)
{
	std::shared_lock<std::shared_mutex> lock(_state_lock);
	// nothing to do before node start
	if(GetState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	// Lower Node is SRTP
	auto node = GetLowerNode();
	if(!node)
	{
		return false;
	}

	auto it = _rtcp_sr_generators.find(rtp_packet->PayloadType());
    if(it != _rtcp_sr_generators.end())
    {
		auto rtcp_sr_generator = it->second;
		
		rtcp_sr_generator->AddRTPPacketAndGenerateRtcpSR(*rtp_packet);
		if(rtcp_sr_generator->IsAvailableRtcpSRPacket())
		{
			auto rtcp_sr_packet = rtcp_sr_generator->PopRtcpSRPacket();
			if(!node->SendData(NodeType::Rtcp, rtcp_sr_packet->GetData()))
			{
				logd("RTCP","Send RTCP failed : pt(%d) ssrc(%u)", rtp_packet->PayloadType(), rtp_packet->Ssrc());
			}
			else
			{
				logd("RTCP", "Send RTCP succeed : pt(%d) ssrc(%u) length(%d)", rtp_packet->PayloadType(), rtp_packet->Ssrc(), rtcp_sr_packet->GetData()->GetLength());
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
	std::shared_lock<std::shared_mutex> lock(_state_lock);
	// nothing to do before node start
	if(GetState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	auto node = GetLowerNode();
	if(!node)
	{
		return false;
	}

	if(!node->SendData(from_node, data))
	{
		loge("RtpRtcp","Send data failed from(%d) data_len(%d)", static_cast<uint16_t>(from_node), data->GetLength());
		return false;
	}

	return true;
}

// Implement Node Interface
// decoded data from srtp
// no upper node( receive data process end)
bool RtpRtcp::OnDataReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data)
{
	std::shared_lock<std::shared_mutex> lock(_state_lock);
	// nothing to do before node start
	if(GetState() != ov::Node::NodeState::Started)
	{
		logtd("Node has not started, so the received data has been canceled.");
		return false;
	}

	if(from_node == NodeType::Srtcp)
	{
		return OnRtcpReceived(data);
	}
	else if(from_node == NodeType::Srtp)
	{
		return OnRtpReceived(data);
	}

    return true;
}

bool RtpRtcp::OnRtpReceived(const std::shared_ptr<const ov::Data> &data)
{
	auto packet = std::make_shared<RtpPacket>(data);
	logtd("%s", packet->Dump().CStr());

	auto track_it = _tracks.find(packet->PayloadType());
	if(track_it == _tracks.end())
	{
		logte("Could not find track info for payload type %d", packet->PayloadType());
		return false;
	}

	auto track = track_it->second;

	if(track->GetMediaType() == cmn::MediaType::Video)
	{
		auto buffer_it = _rtp_jitter_buffers.find(packet->PayloadType());
		if(buffer_it == _rtp_jitter_buffers.end())
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
				logte("Could not get first rtp packet from jitter buffer - payload type : %d", packet->PayloadType());
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
	// Audio
	else
	{
		std::vector<std::shared_ptr<RtpPacket>> rtp_packets;
		rtp_packets.push_back(packet);
		_observer->OnRtpFrameReceived(rtp_packets);
	}

	return true;
}

bool RtpRtcp::OnRtcpReceived(const std::shared_ptr<const ov::Data> &data)
{
	logtd("Get RTCP Packet - length(%d)", data->GetLength());
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
			_observer->OnRtcpReceived(info);
		}
	}
	return true;
}