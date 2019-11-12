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
	_stream = session;

	_audio_configured = audio;
	_timestamp_offset = (uint32_t)rand();
	_sequence_number = (uint16_t)rand();
	_red_sequence_number = (uint16_t)rand();
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

	std::shared_ptr<RtpPacket> rtp_header_template, red_rtp_header_template;
	std::shared_ptr<RtpPacket> last_rtp_header, last_red_rtp_header;

	// 기본 패킷 헤더를 생성
	rtp_header_template = AllocatePacket();
	rtp_header_template->SetTimestamp(rtp_timestamp);

	// 마지막 패킷 헤더를 생성, 마지막 패킷에 extension과 marker=1을 보낸다.
	last_rtp_header = AllocatePacket();
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

		_stream->OnRtpPacketized(packet);

		// RED First
		if(_ulpfec_enabled)
		{
			GenerateRedAndFecPackets(packet);
		}
	}

	return true;
}

bool RtpPacketizer::GenerateRedAndFecPackets(std::shared_ptr<RtpPacket> packet)
{
	// Send RED
	auto red_packet = PackageAsRed(packet);

	// Separate the sequence number of the RED packet from RTP packet.
	// Because FEC packet should not affect the sequence number of the RTP packet.
	AssignSequenceNumber(red_packet.get(), true);

	_ulpfec_generator.AddRtpPacketAndGenerateFec(red_packet);

	// For recovery test
	//if(red_packet->SequenceNumber() % 10 != 0)
	{
		_stream->OnRtpPacketized(red_packet);
	}

	while(_ulpfec_generator.IsAvailableFecPackets())
	{
		auto red_fec_packet = AllocatePacket(true);

		// Timestamp is same as last packet
		red_fec_packet->SetTimestamp(packet->Timestamp());

		// Sequence Number
		//red_fec_packet->SetSequenceNumber(red_packet->SequenceNumber());
		AssignSequenceNumber(red_fec_packet.get(), true);

		_ulpfec_generator.NextPacket(red_fec_packet.get());

		// Send ULPFEC
		_stream->OnRtpPacketized(red_fec_packet);
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

	_stream->OnRtpPacketized(std::move(packet));

	return true;
}

std::shared_ptr<RedRtpPacket> RtpPacketizer::PackageAsRed(std::shared_ptr<RtpPacket> rtp_packet)
{
	return std::make_shared<RedRtpPacket>(_red_payload_type, *rtp_packet);
}

std::shared_ptr<RtpPacket> RtpPacketizer::AllocatePacket(bool ulpfec)
{
	std::shared_ptr<RedRtpPacket> red_packet;
	std::shared_ptr<RtpPacket> rtp_packet;

	if(ulpfec)
	{
		red_packet = std::make_shared<RedRtpPacket>();

		red_packet->SetSsrc(_ssrc);
		red_packet->SetCsrcs(_csrcs);
		red_packet->SetPayloadType(_ulpfec_payload_type);
		red_packet->SetUlpfec(true, _payload_type);

		red_packet->PackageAsRed(_red_payload_type);

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

bool RtpPacketizer::AssignSequenceNumber(RtpPacket *packet, bool red)
{
	if(!red)
	{
		packet->SetSequenceNumber(_sequence_number++);
	}
	else
	{
		packet->SetSequenceNumber(_red_sequence_number++);
	}

	return true;
}

bool RtpPacketizer::MarkerBit(FrameType frame_type, int8_t payload_type)
{
	// Reference: rtp_sender_audio.cc:75
	// temporary code
	return false;
}

