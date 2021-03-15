#pragma once

#include "rtp_rtcp_defines.h"
#include "rtp_packet.h"

class RtpDepacketizingManager
{
public:
	static std::shared_ptr<RtpDepacketizingManager> Create(cmn::MediaCodecId type);

	virtual std::shared_ptr<ov::Data> ParseAndAssembleFrame(std::vector<std::shared_ptr<ov::Data>> payload_list) = 0;
};
