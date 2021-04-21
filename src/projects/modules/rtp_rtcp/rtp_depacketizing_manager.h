#pragma once

#include "rtp_rtcp_defines.h"
#include "rtp_packet.h"

class RtpDepacketizingManager
{
public:
	enum class SupportedDepacketizerType
	{
		H264,
		VP8,
		OPUS,
		MPEG4_GENERIC_AUDIO
	};

	static std::shared_ptr<RtpDepacketizingManager> Create(SupportedDepacketizerType type);
	virtual std::shared_ptr<ov::Data> ParseAndAssembleFrame(std::vector<std::shared_ptr<ov::Data>> payload_list) = 0;
};
