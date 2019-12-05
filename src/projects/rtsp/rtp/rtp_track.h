#pragma once

#include "../rtsp_library.h"
#include "rtp_packet_header.h"

#include <base/common_types.h>

class RtspServer;

class RtpTrack
{
public:
    // TODO(rubu): until the H264 packetizer in the WebRTC end is reworked we must assemble FU-A packets, since the packetizer in the WebRTC
    // end is recreated each time and cannot handle separate FU-A segments in multiple packets
    RtpTrack(RtspServer &rtsp_server_, common::MediaType media_type, common::MediaCodecId media_codec_id, uint32_t stream_id, uint8_t track_id, bool assemble_fu_a_packets = true);
    RtpTrack(const RtpTrack&) = delete;

    virtual ~RtpTrack() = default;

    bool AddRtpPacket(const std::shared_ptr<std::vector<uint8_t>> &rtp_packet);

protected:
    virtual bool AddRtpPayload(const RtpPacketHeader &rtp_packet_header, uint8_t *rtp_payload, size_t rtp_payload_length) = 0;

protected:
    RtspServer &rtsp_server_;
    common::MediaType media_type_;
    common::MediaCodecId media_codec_id_; 
    uint32_t stream_id_;
    uint8_t track_id_;
    bool first_packet_ = true;
    uint32_t first_timestamp_;
};