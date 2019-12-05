#include "rtp_track.h"
#include "../rtsp_server.h"

RtpTrack::RtpTrack(RtspServer &rtsp_server,
    common::MediaType media_type,
    common::MediaCodecId media_codec_id,
    uint32_t stream_id,
    uint8_t track_id,
    bool assemble_fu_a_packets) : rtsp_server_(rtsp_server),
    media_type_(media_type),
    media_codec_id_(media_codec_id),
    stream_id_(stream_id),
    track_id_(track_id)
{
}

bool RtpTrack::AddRtpPacket(const std::shared_ptr<std::vector<uint8_t>> &rtp_packet)
{
        const uint8_t (&rtp_header_bytes)[RtpPacketHeaderSize] = reinterpret_cast<const uint8_t(&)[RtpPacketHeaderSize]>(*rtp_packet->data());
        auto rtp_packet_header = RtpPacketHeaderFromData(rtp_header_bytes);
        if (first_packet_)
        {
            first_timestamp_ = rtp_packet_header.timestamp_;
            first_packet_ = false;
        }
        auto payload_offset = sizeof(RtpPacketHeader) + rtp_packet_header.csrc_count_ * sizeof(uint32_t);
        auto *rtp_payload = rtp_packet->data() + payload_offset;
        return AddRtpPayload(rtp_packet_header, rtp_payload, rtp_packet->size() - payload_offset);
}