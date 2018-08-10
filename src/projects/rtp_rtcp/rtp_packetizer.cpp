#include "rtp_packetizer.h"
#include "rtp_packetizer_vp8.h"

RtpPacketizer *RtpPacketizer::Create(RtpVideoCodecType type,
                                     size_t max_payload_len,
                                     size_t last_packet_reduction_len,
                                     const RTPVideoTypeHeader *rtp_type_header,
                                     FrameType frame_type)
{
	switch(type)
	{
		case RtpVideoCodecType::Vp8:
			return new RtpPacketizerVp8(rtp_type_header->vp8, max_payload_len, last_packet_reduction_len);

		case RtpVideoCodecType::None:
			break;
	}

	loge("rtp_rtcp", "RtpPacketizer::Create - Cannot create packizer");
	loge("rtp_rtcp", "Video type : %d | Max payload len : %d | Last packet reduction len : %d",
	     type, max_payload_len, last_packet_reduction_len);

	return nullptr;
}
