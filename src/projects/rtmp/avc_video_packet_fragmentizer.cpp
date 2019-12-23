#include "avc_video_packet_fragmentizer.h"

// https://www.adobe.com/content/dam/acom/en/devnet/flv/video_file_format_spec_v10.pdf

#define OV_LOG_TAG "avcvideopacketfragmentizer"

constexpr size_t avc_video_packet_payload_offset = 5;
constexpr size_t avc_decoder_configuration_nal_length_offset = 4;
constexpr size_t avc_decoder_configuration_sps_count_offset = 5;

static uint32_t UnpackNalLength(const uint8_t *buffer, uint8_t nal_length_size)
{
    uint32_t nal_length = 0;
    uint8_t nal_length_bytes_remaining = nal_length_size;
    while (nal_length_bytes_remaining > 0)
    {
        nal_length <<= 8;
        nal_length += *(buffer++);
        --nal_length_bytes_remaining;
    }
    return nal_length;
}

static const uint8_t *ParseNalUnits(const uint8_t *nal_unit_buffer,
    size_t length,
    uint8_t nal_length_size,
    std::vector<std::pair<size_t, size_t>> &fragments, 
    size_t offset,
    size_t &continuation_length,
    size_t nal_unit_count = 0)
{
    if (nal_length_size == 0) return nullptr;
    size_t nal_units_parsed = 0;

    while (length && (nal_unit_count == 0 || nal_units_parsed < nal_unit_count))
    {
        if (length < nal_length_size)
        {
            return nullptr;
        }
        const auto nal_length = UnpackNalLength(nal_unit_buffer, nal_length_size);
        length -= nal_length_size;
        offset += nal_length_size;
        nal_unit_buffer += nal_length_size;
        if (length < nal_length)
        {
            continuation_length = nal_length - length;
            fragments.emplace_back(offset, length);
            return nal_unit_buffer + length;
        }
        fragments.emplace_back(offset, nal_length);
        length -= nal_length;
        offset += nal_length;
        nal_unit_buffer += nal_length;
        ++nal_units_parsed;
    }
    return nal_unit_buffer;
}

std::unique_ptr<FragmentationHeader> AvcVideoPacketFragmentizer::FromAvcVideoPacket(const uint8_t *packet, size_t length)
{
    if (length < 5) return nullptr; // No need to even parse the packet header, since there is no payload

    size_t base_offset = 0;
    const uint8_t frame_type = (packet[0] >> 4) & 0xf, codec_id = packet[0] & 0xf; 
    std::unique_ptr<FragmentationHeader> fragmentation_header;
    std::vector<std::pair<size_t, size_t>> fragments;
    bool last_fragment_complete = true;
    if (codec_id == 7 && (frame_type == 1 || frame_type == 2))
    {
        const uint8_t avc_packet_type = packet[1];
        base_offset += avc_video_packet_payload_offset;
        const uint8_t *avc_packet_payload = packet + avc_video_packet_payload_offset;
        switch (avc_packet_type)
        {
        case 0:
            // AVCDecoderConfigurationRecord
            //
            // The record starts with configuration version, the box type/size fields are not present
            {
                nal_length_size_ = ((*(avc_packet_payload + avc_decoder_configuration_nal_length_offset)) & 0b11) + 1;
                const uint8_t sps_count = (*(avc_packet_payload + avc_decoder_configuration_sps_count_offset)) & 0b11111;
                base_offset += avc_decoder_configuration_sps_count_offset;
                avc_packet_payload += avc_decoder_configuration_sps_count_offset + 1;
                size_t continuation_length = 0;
                if (sps_count)
                {
                    const auto *result = ParseNalUnits(avc_packet_payload, length - base_offset, 2, fragments, base_offset, continuation_length, sps_count);
                    if (result == nullptr)
                    {
                        OV_ASSERT2(false);
                        return nullptr;
                    }
                    base_offset += result - avc_packet_payload;
                    avc_packet_payload = result;
                }
                const uint8_t pps_count = *(avc_packet_payload);
                base_offset += 1;
                avc_packet_payload += 1;
                if (pps_count)
                {
                    if (ParseNalUnits(avc_packet_payload, length - base_offset, 2, fragments, base_offset, continuation_length, pps_count) == nullptr)
                    {
                        OV_ASSERT2(false);
                        return nullptr;
                    }

                }
            }
            break;
        case 1:
            // One or more NALUs
            if (nal_length_size_ == 0) return nullptr;

            if (continuation_length_)
            {
                size_t continuation_length = std::min(continuation_length_, length - base_offset);
                fragments.emplace_back(base_offset, continuation_length);
                avc_packet_payload += continuation_length;
                base_offset += continuation_length;
                continuation_length_ -= continuation_length;
                last_fragment_complete = continuation_length_ == 0;
                if (continuation_length_ !=0 || continuation_length == length - base_offset)
                {
                    break;
                }
            }

            if (ParseNalUnits(avc_packet_payload, length - base_offset, nal_length_size_, fragments, base_offset, continuation_length_) == nullptr)
            {
                OV_ASSERT2(false);
                return nullptr;
            }
            last_fragment_complete = continuation_length_ == 0;
            break;
        }    
    }
    if (fragments.empty() == false)
    {
        fragmentation_header = std::make_unique<FragmentationHeader>();
        for (size_t fragment_index = 0; fragment_index < fragments.size(); ++fragment_index)
        {
            fragmentation_header->fragmentation_offset.emplace_back(fragments[fragment_index].first);
            fragmentation_header->fragmentation_length.emplace_back(fragments[fragment_index].second);
        }
        fragmentation_header->last_fragment_complete = last_fragment_complete;
    }
    return fragmentation_header;
}