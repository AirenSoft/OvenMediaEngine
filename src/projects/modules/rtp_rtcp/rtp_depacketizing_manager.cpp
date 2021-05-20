#include "rtp_depacketizing_manager.h"

#include "rtp_depacketizer_generic_audio.h"
#include "rtp_depacketizer_mpeg4_generic_audio.h"
#include "rtp_depacketizer_h264.h"
#include "rtp_depacketizer_vp8.h"

std::shared_ptr<RtpDepacketizingManager> RtpDepacketizingManager::Create(SupportedDepacketizerType type)
{
	switch(type)
	{
		case RtpDepacketizingManager::SupportedDepacketizerType::H264:
			return std::make_shared<RtpDepacketizerH264>();
		case RtpDepacketizingManager::SupportedDepacketizerType::VP8:
			return std::make_shared<RtpDepacketizerVP8>();
		case RtpDepacketizingManager::SupportedDepacketizerType::MPEG4_GENERIC_AUDIO:
			return std::make_shared<RtpDepacketizerMpeg4GenericAudio>();
		case RtpDepacketizingManager::SupportedDepacketizerType::OPUS:
			return std::make_shared<RtpDepacketizerGenericAudio>();
		default:
			// Not supported
			break;
	}

	return nullptr;
}
