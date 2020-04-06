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

bool AvcVideoPacketFragmentizer::MakeHeader(const std::shared_ptr<MediaPacket> &packet)
{
    auto fragment_header = packet->GetFragHeader();

    const uint8_t* srcData = static_cast<const uint8_t*>(packet->GetData()->GetData());
    size_t dataOffset = 0;
    size_t dataSize = packet->GetData()->GetLength();
    // size_t previous_offset = -1;

    std::vector<std::pair<size_t, size_t>> offset_list;

    // NAL 헤더의 START_CODE를 탐색하여, START 코드 정보를 제외한 NAL 패킷의 위치 정보를 리스트로 만든다.
    while ( dataOffset < dataSize )
    {
        size_t remainDataSize = dataSize - dataOffset;
        const uint8_t* data = srcData + dataOffset;

        if (remainDataSize >= 3 &&
                0x00 == data[0] &&
                0x00 == data[1] &&
                0x01 == data[2])
        {
            offset_list.emplace_back(dataOffset, 3); // Offset, SIZEOF(START_CODE[3])
            dataOffset += 3;
        }
        else if (remainDataSize >= 4 &&
                 0x00 == data[0] &&
                 0x00 == data[1] &&
                 0x00 == data[2] &&
                 0x01 == data[3])
        {
            offset_list.emplace_back(dataOffset, 4); // Offset, SIZEOF(START_CODE[4])
            dataOffset += 4;
        }
        else
        {
            dataOffset += 1;
        }
    }

    fragment_header->Clear();


    for (size_t index = 0; index < offset_list.size(); ++index)
    {
        size_t nalu_offset = 0;
        size_t nalu_data_len = 0;

        if (index != offset_list.size() - 1)
        {
            nalu_offset = offset_list[index].first + offset_list[index].second;
            nalu_data_len = offset_list[index + 1].first - nalu_offset;
        }
        else
        {
            nalu_offset = offset_list[index].first + offset_list[index].second;
            nalu_data_len = dataSize - nalu_offset;
        }

#if 0
        uint8_t nalu_header = *(srcData + nalu_offset);
        uint8_t forbidden_zero_bit = (nalu_header >> 7)  & 0x01;
        uint8_t nal_ref_idc = (nalu_header >> 5)  & 0x03;
        uint8_t nal_unit_type = (nalu_header)  & 0x01F;

        // if( (nal_unit_type == (uint8_t)AvcNaluType::IDR) || (nal_unit_type == (uint8_t)AvcNaluType::SEI) || (nal_unit_type == (uint8_t)AvcNaluType::SPS) || (nal_unit_type == (uint8_t)AvcNaluType::PPS))
        {
            logtd("[%d][%d] nal_ref_idc:%2d, nal_unit_type:%2d => offset:%d, nalu_size:%d, nalu_offset:%d, nalu_length:%d"
            , index
            , dataSize
            , nal_ref_idc, nal_unit_type
            , offset_list[index].first
            , offset_list[index].second
            , nalu_offset
            , nalu_data_len
                 );
        }
#endif

        // if (nal_unit_type == 6) // SEI
        //     logtd("%s", ov::Dump(srcData+nalu_offset, nalu_data_len, 32).CStr());

        fragment_header->fragmentation_offset.emplace_back(nalu_offset);
        fragment_header->fragmentation_length.emplace_back(nalu_data_len);
    }

    return true;
}
