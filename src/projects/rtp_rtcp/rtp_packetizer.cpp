#include <time.h>
#include <iostream>
#include <memory>
#include "rtp_packet.h"
#include "red_rtp_packet.h"
#include "rtp_packetizing_manager.h"
#include "rtp_packetizer.h"

#define OV_LOG_TAG "RtpRtcp"

RtpPacketizer::RtpPacketizer(bool audio, std::shared_ptr<RtpRtcpPacketizerInterface> session)
{
	_session = session;

	_audio_configured = audio;
	_timestamp_offset = (uint32_t)rand();
	_sequence_number = (uint16_t)rand();
	_ulpfec_enabled = false;
}

RtpPacketizer::~RtpPacketizer()
{

}

void RtpPacketizer::SetPayloadType(uint8_t payload_type)
{
	_payload_type = payload_type;
}

void RtpPacketizer::SetSSRC(const uint32_t ssrc)
{
	_ssrc = ssrc;
}

void RtpPacketizer::SetCsrcs(const std::vector<uint32_t> &csrcs)
{
	_csrcs = csrcs;
}

void RtpPacketizer::SetUlpfec(uint8_t red_payload_type, uint8_t ulpfec_payload_type)
{
	_ulpfec_enabled = true;
	_red_payload_type = red_payload_type;
	_ulpfec_payload_type = ulpfec_payload_type;
}

bool RtpPacketizer::Packetize(FrameType frame_type,
                                   uint32_t timestamp,
                                   const uint8_t *payload_data,
                                   size_t payload_size,
                                   const FragmentationHeader *fragmentation,
                                   const RTPVideoHeader *rtp_header)
{
	// TODO: 현재 시간을 넣어서 생성하지만 향후에는 Capture한 당시 Timestamp를 넣는다.
	// uint32_t rtp_timestamp = _timestamp_offset + (uint32_t)time(nullptr);
	uint32_t rtp_timestamp = timestamp;

	// Audio, Video 분기 한다.
	if(_audio_configured)
	{
		return PacketizeAudio(frame_type, rtp_timestamp, payload_data, payload_size);
	}
	else
	{
		return PacketizeVideo(rtp_header->codec, frame_type, rtp_timestamp, payload_data, payload_size, fragmentation, rtp_header);
	}
}

bool RtpPacketizer::PacketizeVideo(RtpVideoCodecType video_type,
                                   FrameType frame_type,
                                   uint32_t rtp_timestamp,
                                   const uint8_t *payload_data,
                                   size_t payload_size,
                                   const FragmentationHeader *fragmentation,
                                   const RTPVideoHeader *video_header)
{

	std::shared_ptr<RtpPacket> rtp_header_template;
	std::shared_ptr<RtpPacket> last_rtp_header;

	// 기본 패킷 헤더를 생성
	rtp_header_template = AllocatePacket(_ulpfec_enabled, false);
	rtp_header_template->SetTimestamp(rtp_timestamp);

	// 마지막 패킷 헤더를 생성, 마지막 패킷에 extension과 marker=1을 보낸다.
	last_rtp_header = AllocatePacket(_ulpfec_enabled, false);
	last_rtp_header->SetTimestamp(rtp_timestamp);

	// TODO: 향후 다음 Extension을 추가한다.
	// Rotation Extension
	// Video Content Type Extension
	// Video Timing Extension

	// -20 is for FEC
	size_t max_data_payload_length = DEFAULT_MAX_PACKET_SIZE - rtp_header_template->HeadersSize() - 100;
	size_t last_packet_reduction_len = last_rtp_header->HeadersSize() - rtp_header_template->HeadersSize();

	// Packetizer 생성
	std::unique_ptr<RtpPacketizingManager> packetizer(RtpPacketizingManager::Create(video_type,
	                                                                max_data_payload_length,
	                                                                last_packet_reduction_len,
	                                                                video_header ? &(video_header->codec_header) : nullptr, frame_type));
	if(!packetizer)
	{
		// 지원하지 못하는 Frame이 들어옴, critical error
		logte("Cannot create _packetizers");
		return false;
	}

	// Paketizer에 Payload를 셋팅
	size_t num_packets = packetizer->SetPayloadData(payload_data, payload_size, fragmentation);
	if(num_packets == 0)
	{
		logte("Packetizer returns 0 packet");
		return false;
	}

	// 생성된 Packet 만큼 전송한다.
	// 마지막 패킷은 특수하게 처리한다. (Extension 포함, Marker=1)
	for(size_t i = 0; i < num_packets; ++i)
	{
		bool last = (i + 1) == num_packets;
		auto packet = last ? std::move(last_rtp_header) : std::make_shared<RtpPacket>(*rtp_header_template);

		if(!packetizer->NextPacket(packet.get()))
		{
			return false;
		}

		if(!AssignSequenceNumber(packet.get()))
		{
			return false;
		}

		// RtcSession을 통해 전송한다.
		// GTEST
		if(packet->SequenceNumber() % 10 != 0)
		{
			_session->OnRtpPacketized(packet);
		}

		if(_ulpfec_enabled)
		{
			GenerateUlpfecPackets(std::static_pointer_cast<RedRtpPacket>(packet));
		}
	}

	return true;
}

bool RtpPacketizer::GenerateUlpfecPackets(std::shared_ptr<RedRtpPacket> packet)
{
	_ulpfec_generator.AddRtpPacketAndGenerateFec(packet);

	uint16_t i = 0;
	while(_ulpfec_generator.IsAvailableFecPackets())
	{
		auto red_fec_packet = AllocatePacket(true, true);

		// Timestamp is same as last packet
		red_fec_packet->SetTimestamp(packet->Timestamp());

		// Sequence Number
		red_fec_packet->SetSequenceNumber(_sequence_number++);

		_ulpfec_generator.NextPacket(red_fec_packet.get());

		_session->OnRtpPacketized(red_fec_packet);
		i++;
	}

	return true;
}

bool RtpPacketizer::PacketizeAudio(FrameType frame_type,
                                   uint32_t rtp_timestamp,
                                   const uint8_t *payload_data,
                                   size_t payload_size)
{
	if(payload_size == 0 || payload_data == nullptr)
	{
		if(frame_type == FrameType::EmptyFrame)
		{
			// Don't need to send empty packet
			return true;
		}

		return false;
	}

	std::shared_ptr<RtpPacket> packet = AllocatePacket();
	packet->SetMarker(MarkerBit(frame_type, _payload_type));
	packet->SetPayloadType(_payload_type);
	packet->SetTimestamp(rtp_timestamp);

	uint8_t *payload = packet->AllocatePayload(payload_size);

	if(payload == nullptr)
	{
		OV_ASSERT2(false);
		return false;
	}

	::memcpy(payload, payload_data, payload_size);

	if(AssignSequenceNumber(packet.get()) == false)
	{
		OV_ASSERT2(false);
		return false;
	}

	// logd("RtpSender.Packet", "Trying to send packet:\n%s", packet->GetData()->Dump().CStr());

	_session->OnRtpPacketized(std::move(packet));

	return true;
}

std::shared_ptr<RtpPacket> RtpPacketizer::AllocatePacket(bool red, bool ulpfec)
{
	std::shared_ptr<RedRtpPacket> red_packet;
	std::shared_ptr<RtpPacket> rtp_packet;

	if(red)
	{
		red_packet = std::make_shared<RedRtpPacket>();
		red_packet->SetSsrc(_ssrc);
		red_packet->SetCsrcs(_csrcs);

		if(ulpfec)
		{
			red_packet->SetPayloadType(_ulpfec_payload_type);
		}
		else
		{
			red_packet->SetPayloadType(_payload_type);
		}

		red_packet->PackageRed(_red_payload_type);

		return red_packet;
	}
	else
	{
		rtp_packet = std::make_shared<RtpPacket>();
		rtp_packet->SetSsrc(_ssrc);
		rtp_packet->SetCsrcs(_csrcs);
		rtp_packet->SetPayloadType(_payload_type);

		return rtp_packet;
	}
}

bool RtpPacketizer::AssignSequenceNumber(RtpPacket *packet)
{
	packet->SetSequenceNumber(_sequence_number++);

	return true;
}

bool RtpPacketizer::MarkerBit(FrameType frame_type, int8_t payload_type)
{
	// Reference: rtp_sender_audio.cc:75
	// temporary code
	return false;
}

