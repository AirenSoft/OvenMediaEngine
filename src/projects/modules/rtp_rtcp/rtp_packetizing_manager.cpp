#include "rtp_packetizing_manager.h"
#include "rtp_packetizer_vp8.h"
#include "rtp_packetizer_h264.h"

std::shared_ptr<RtpPacketizingManager> RtpPacketizingManager::Create(RtpVideoCodecType type)
{
	switch(type)
	{
		case RtpVideoCodecType::Vp8:
			return std::move(std::make_shared<RtpPacketizerVp8>());

		case RtpVideoCodecType::H264:
			return std::move(std::make_shared<RtpPacketizerH264>());

		case RtpVideoCodecType::None:
			break;
	}

	loge("rtp_rtcp", "RtpPacketizingManager::Create - Cannot create packizer");
	loge("rtp_rtcp", "Video type : %d", type);

	return nullptr;
}
