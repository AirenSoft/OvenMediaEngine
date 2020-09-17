#include "rtcp_sr_generator.h"

RtcpSRGenerator::RtcpSRGenerator(uint32_t ssrc)
{
    _ssrc = ssrc;
    _created_time = std::chrono::system_clock::now();
    _last_generated_time = std::chrono::system_clock::now();
}

void RtcpSRGenerator::AddRTPPacketAndGenerateRtcpSR(const RtpPacket &rtp_packet)
{
    //logc("DEBUG", ">>>> timestamp(%u) ssrc(%u)", rtp_packet.Timestamp(), rtp_packet.Ssrc());
    _packet_count ++;
    _octec_count += rtp_packet.PayloadSize();

    // RTCP Interval
    // Send RTCP SR twice a second for the first 10 seconds so the player can sync AV. 
    // After 5 seconds, send RTCP SR once every 5 seconds. 
    if((GetElapsedTimeMSFromCreated() < 10000 && GetElapsedTimeMSFromRtcpSRGenerated() > 500) ||
        GetElapsedTimeMSFromRtcpSRGenerated() > 4999)
    {
        _rtcp_sr_packet = RtcpPacket::MakeSrPacket(_ssrc, rtp_packet.Timestamp(), _packet_count, _octec_count);

        // Reset RTCP information
        _packet_count = 0;
        _octec_count = 0;
        _last_generated_time = std::chrono::system_clock::now();
        _rtcp_generated_count++;
    }
}

bool RtcpSRGenerator::IsAvailableRtcpSRPacket() const
{
    return (_rtcp_sr_packet != nullptr);
}

std::shared_ptr<ov::Data> RtcpSRGenerator::PopRtcpSRPacket()
{
    if(_rtcp_sr_packet == nullptr)
    {
        return nullptr;
    }

    return std::move(_rtcp_sr_packet);
}

uint32_t RtcpSRGenerator::GetElapsedTimeMSFromCreated()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _created_time).count();
}

uint32_t RtcpSRGenerator::GetElapsedTimeMSFromRtcpSRGenerated()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _last_generated_time).count();
}