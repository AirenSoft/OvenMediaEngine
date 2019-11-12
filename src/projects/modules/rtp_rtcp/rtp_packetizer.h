#pragma once

#include "rtp_packet.h"
#include "rtp_packetizing_manager.h"
#include "rtp_rtcp_defines.h"
#include "ulpfec_generator.h"
#include <memory>

class RtpPacketizer
{
public:
	RtpPacketizer(bool audio, std::shared_ptr<RtpRtcpPacketizerInterface> session);
	~RtpPacketizer();

	void SetUlpfec(uint8_t _red_payload_type, uint8_t _ulpfec_payload_type);
	void SetPayloadType(uint8_t payload_type);
	void SetSSRC(uint32_t ssrc);
	void SetCsrcs(const std::vector<uint32_t> &csrcs);

	// RTP Packet
	bool Packetize(FrameType frame_type,
	               uint32_t timestamp,
	               const uint8_t *payload_data,
	               size_t payload_size,
	               const FragmentationHeader *fragmentation,
	               const RTPVideoHeader *rtp_header);

private:
	// Basic
	std::shared_ptr<RtpPacket> AllocatePacket(bool ulpfec=false);
	std::shared_ptr<RedRtpPacket> PackageAsRed(std::shared_ptr<RtpPacket> rtp_packet);
	bool AssignSequenceNumber(RtpPacket *packet, bool red = false);
	bool MarkerBit(FrameType frame_type, int8_t payload_type);

	// Video Packet Sender Interface
	bool PacketizeVideo(RtpVideoCodecType video_type,
	                    FrameType frame_type,
	                    uint32_t rtp_timestamp,
	                    const uint8_t *payload_data,
	                    size_t payload_size,
	                    const FragmentationHeader *fragmentation,
	                    const RTPVideoHeader *video_header);

	bool GenerateRedAndFecPackets(std::shared_ptr<RtpPacket> packet);

	// Audio Pakcet Sender Interface
	bool PacketizeAudio(FrameType frame_type,
	                    uint32_t rtp_timestamp,
	                    const uint8_t *payload_data,
	                    size_t payload_size);

	// Member
	uint32_t _timestamp_offset;
	bool _audio_configured;

	// Session Information
	uint32_t _ssrc;
	uint8_t _payload_type;
	std::vector<uint32_t> _csrcs;
	// Sequence Number
	uint16_t _sequence_number;
	uint16_t _red_sequence_number;

	bool _ulpfec_enabled;
	uint8_t _red_payload_type;
	uint8_t _ulpfec_payload_type;

	UlpfecGenerator _ulpfec_generator;

	// Session Descriptor
	std::shared_ptr<RtpRtcpPacketizerInterface> _stream;
};
