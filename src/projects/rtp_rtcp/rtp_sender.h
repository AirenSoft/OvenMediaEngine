#pragma once

#include "rtp_packet.h"
#include "rtp_packetizer.h"
#include "rtp_rtcp_defines.h"
#include <memory>

class RTPSender
{
public:
	RTPSender(bool audio, std::shared_ptr<RtpRtcpSession> session);
	~RTPSender();

	void SetPayloadType(uint8_t payload_type);
	void SetSSRC(uint32_t ssrc);
	void SetCsrcs(const std::vector<uint32_t> &csrcs);

	// RTP Packet
	bool SendOutgoingData(FrameType frame_type,
	                      uint32_t timestamp,
	                      const uint8_t *payload_data,
	                      size_t payload_size,
	                      const FragmentationHeader *fragmentation,
	                      const RTPVideoHeader *rtp_header);

private:
	// Basic
	std::unique_ptr<RtpPacket> AllocatePacket();
	bool AssignSequenceNumber(RtpPacket *packet);

	bool MarkerBit(FrameType frame_type, int8_t payload_type);

	// Video Packet Sender Interface
	bool SendVideo(RtpVideoCodecType video_type,
	               FrameType frame_type,
	               uint32_t rtp_timestamp,
	               const uint8_t *payload_data,
	               size_t payload_size,
	               const FragmentationHeader *fragmentation,
	               const RTPVideoHeader *video_header);

	// Audio Pakcet Sender Interface
	bool SendAudio(FrameType frame_type,
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

	// Session Descriptor
	std::shared_ptr<RtpRtcpSession> _session;
};
