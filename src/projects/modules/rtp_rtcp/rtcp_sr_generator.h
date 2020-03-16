#pragma once

#include "base/common_types.h"
#include "rtp_packet.h"
#include "rtcp_packet.h"

class RtcpSRGenerator
{
public:
    RtcpSRGenerator(uint32_t ssrc);

	void AddRTPPacketAndGenerateRtcpSR(const RtpPacket &rtp_packet);
	bool IsAvailableRtcpSRPacket() const;
	std::shared_ptr<ov::Data>   PopRtcpSRPacket();
	
private:
	uint32_t GetElapsedTimeMSFromCreated();
	uint32_t GetElapsedTimeMSFromRtcpSRGenerated();

    uint32_t	_ssrc = 0;
	uint32_t	_last_timestamp = 0;
    uint32_t    _rtcp_generated_count = 0;
    uint32_t    _packet_count = 0;
    uint32_t    _octec_count = 0;

	std::chrono::system_clock::time_point _created_time;
    std::chrono::system_clock::time_point _last_generated_time;

	// It will be changed to RtcpSRPacket class, ov::Data is used temporarily because RtcpSRPacket is not available now. 
	std::shared_ptr<ov::Data>	_rtcp_sr_packet = nullptr;
};