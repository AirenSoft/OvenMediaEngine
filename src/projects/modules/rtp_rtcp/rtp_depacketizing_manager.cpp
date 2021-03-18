#include "rtp_depacketizing_manager.h"
#include "rtp_depacketizer_generic_audio.h"
#include "rtp_depacketizer_h264.h"
#include "rtp_depacketizer_vp8.h"

std::shared_ptr<RtpDepacketizingManager> RtpDepacketizingManager::Create(cmn::MediaCodecId type)
{
	switch(type)
	{
		case cmn::MediaCodecId::H264:
			return std::move(std::make_shared<RtpDepacketizerH264>());
		case cmn::MediaCodecId::Vp8:
			return std::move(std::make_shared<RtpDepacketizerVP8>());
		case cmn::MediaCodecId::Opus:
			return std::move(std::make_shared<RtpDepacketizerGenericAudio>());
		default:
			// Not supported
			break;
	}

	loge("rtp_rtcp", "Cannot create %s RTP depacketizer", ::StringFromMediaCodecId(type).CStr());

	return nullptr;
}
