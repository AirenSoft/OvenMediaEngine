#include "rtp_tcp_track.h"
#include "../rtsp_server.h"
#include "h264/h264_nal_unit_types.h"

#define OV_LOG_TAG "RtpTcpTrack"

#if defined(PARANOID_STREAM_VALIDATION)
#include "h264/h264_nal_unit_bitstream_parser.h"
#endif

RtpTcpTrack::RtpTcpTrack(RtspServer& rtsp_server,
    common::MediaType media_type,
    common::MediaCodecId media_codec_id,
    uint32_t stream_id, 
    uint8_t track_id,
    uint16_t rtp_channel,
    uint16_t rtcp_channel) : RtpTrack(rtsp_server, media_type, media_codec_id, stream_id, track_id),
    rtp_channel_(rtp_channel),
    rtcp_channel_(rtcp_channel)
{
}


bool RtpTcpTrack::AddPacket(uint8_t channel, const std::shared_ptr<std::vector<uint8_t>> &rtp_packet)
{
    // Currently we skip RTCP
    if (channel == rtp_channel_)
    {
        return AddRtpPacket(rtp_packet);
    }
    return true;
}