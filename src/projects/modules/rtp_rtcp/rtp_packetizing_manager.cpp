#include "rtp_packetizing_manager.h"
#include "rtp_packetizer_vp8.h"
#include "rtp_packetizer_h264.h"

RtpPacketizingManager *RtpPacketizingManager::Create(RtpVideoCodecType type,
                                     size_t max_payload_len,
                                     size_t last_packet_reduction_len,
                                     const RTPVideoTypeHeader *rtp_type_header,
                                     FrameType frame_type)
{
	switch(type)
	{
		case RtpVideoCodecType::Vp8:
			return new RtpPacketizerVp8(rtp_type_header->vp8, max_payload_len, last_packet_reduction_len);

		case RtpVideoCodecType::H264:
			return new RtpPacketizerH264(max_payload_len, last_packet_reduction_len, rtp_type_header->h264.packetization_mode);

		case RtpVideoCodecType::None:
			break;
	}

	loge("rtp_rtcp", "RtpPacketizingManager::Create - Cannot create packizer");
	loge("rtp_rtcp", "Video type : %d | Max Payload len : %d | Last packet reduction len : %d",
	     type, max_payload_len, last_packet_reduction_len);

	return nullptr;
}
