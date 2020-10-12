#include "rtp_packetizing_manager.h"
#include "rtp_packetizer_vp8.h"
#include "rtp_packetizer_h264.h"
#include "rtp_packetizer_h265.h"

#include <base/ovlibrary/converter.h>

std::shared_ptr<RtpPacketizingManager> RtpPacketizingManager::Create(common::MediaCodecId type)
{
	switch(type)
	{
		case common::MediaCodecId::Vp8:
			return std::move(std::make_shared<RtpPacketizerVp8>());

		case common::MediaCodecId::H264:
			return std::move(std::make_shared<RtpPacketizerH264>());

		case common::MediaCodecId::H265:
			return std::move(std::make_shared<RtpPacketizerH265>());

		default:
			// Not supported
			break;
	}

	loge("rtp_rtcp", "Cannot create %s RTP packetizer", ov::Converter::ToString(type).CStr());

	return nullptr;
}
