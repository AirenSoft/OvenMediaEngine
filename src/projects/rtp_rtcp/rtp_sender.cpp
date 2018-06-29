
#include <time.h>
#include <iostream>
#include <memory>
#include "rtp_packet.h"
#include "rtp_packetizer.h"
#include "rtp_sender.h"

RTPSender::RTPSender(bool audio, std::shared_ptr<RtpRtcpSession> session)
{
	_session = session;

	_audio_configured = audio;
	_timestamp_offset = (uint32_t)rand();
	_sequence_number = (uint16_t)rand();
}

RTPSender::~RTPSender()
{

}

bool RTPSender::SendOutgoingData(FrameType frame_type,
                                uint32_t timestamp,
                                const uint8_t* payload_data,
                                size_t payload_size,
                                const FragmentationHeader* fragmentation,
                                const RTPVideoHeader* rtp_header)
{
	// TODO: 현재 시간을 넣어서 생성하지만 향후에는 Capture한 당시 Timestamp를 넣는다.
	// uint32_t rtp_timestamp = _timestamp_offset + (uint32_t)time(nullptr);
	uint32_t rtp_timestamp = timestamp;

	// Audio, Video 분기 한다.
	if (_audio_configured)
	{
		// TODO: Audio는 향후 구현
	}
	else
	{
		// TODO: payload_type을 보고 설정해야 한다.
		RtpVideoCodecTypes video_type = kRtpVideoVp8;

		return SendVideo(video_type, frame_type, rtp_timestamp, payload_data, payload_size,
						fragmentation, rtp_header);
	}
}

bool RTPSender::SendVideo( RtpVideoCodecTypes video_type,
                            FrameType frame_type,
                            uint32_t rtp_timestamp,
                            const uint8_t* payload_data,
                            size_t payload_size,
                            const FragmentationHeader* fragmentation,
                            const RTPVideoHeader* video_header)
{
//	logd("rtp_rtcp", "RTPSender::SendVideo Enter\n");

	std::unique_ptr<RtpPacket> rtp_header_template, last_rtp_header;

	// 기본 패킷 헤더를 생성
	rtp_header_template = AllocatePacket();
	rtp_header_template->SetPayloadType(_payload_type);
	rtp_header_template->SetTimestamp(rtp_timestamp);

	// 마지막 패킷 헤더를 생성, 마지막 패킷에 extension과 marker=1을 보낸다.
	last_rtp_header = AllocatePacket();
	last_rtp_header->SetPayloadType(_payload_type);
	last_rtp_header->SetTimestamp(rtp_timestamp);
	// TODO: 향후 다음 Extension을 추가한다.
		// Rotation Extension
		// Video Content Type Extension
		// Video Timing Extension

	size_t max_data_payload_length = DEFAULT_MAX_PACKET_SIZE - rtp_header_template->HeadersSize() - 10;
	size_t last_packet_reduction_len = last_rtp_header->HeadersSize() - rtp_header_template->HeadersSize();

	// Packetizer 생성
	std::unique_ptr<RtpPacketizer> packetizer(RtpPacketizer::Create(video_type,
											  max_data_payload_length,
											  last_packet_reduction_len,
											  video_header ? &(video_header->codecHeader) : nullptr, frame_type));
	if (!packetizer)
	{
		// 지원하지 못하는 Frame이 들어옴, critical error
        loge("RTP_RTCP","Cannot create packetizer");
		return false;
	}

	// Paketizer에 Payload를 셋팅
	size_t num_packets = packetizer->SetPayloadData(payload_data, payload_size, fragmentation);
	if (num_packets == 0)
	{
        loge("RTP_RTCP","Packetizer returns 0 packet");
		return false;
	}
	
	// 생성된 Packet 만큼 전송한다.
	// 마지막 패킷은 특수하게 처리한다. (Extension 포함, Marker=1)
	for (size_t i = 0; i < num_packets; ++i)
	{
		bool last = (i + 1) == num_packets;
		auto packet = last ? std::move(last_rtp_header) : std::make_unique<RtpPacket>(*rtp_header_template.get());

		if (!packetizer->NextPacket(packet.get()))
		{
			return false;
		}

		if (!AssignSequenceNumber(packet.get()))
		{
			return false;
		}

		// RtcSession을 통해 전송한다.
		_session->SendRtpToNetwork(std::move(packet));
	}

	return true;
}

std::unique_ptr<RtpPacket> RTPSender::AllocatePacket()
{
	std::unique_ptr<RtpPacket> packet(new RtpPacket());

	packet->SetSsrc(_ssrc);
	packet->SetCsrcs(_csrcs);

	return packet;
}

bool RTPSender::AssignSequenceNumber(RtpPacket* packet)
{
	packet->SetSequenceNumber(_sequence_number++);
	return true;
}

void RTPSender::SetPayloadType(const uint8_t pt)
{
	_payload_type = pt;
}

void RTPSender::SetSSRC(const uint32_t ssrc)
{
	_ssrc = ssrc;
}

void RTPSender::SetCsrcs(const std::vector<uint32_t>& csrcs)
{
	_csrcs = csrcs;
}
