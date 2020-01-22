#include "h264_slice_types.h"

int16_t GetSliceTypeNalRefIdc(uint8_t slice_type)
{
    if (5 <= slice_type && slice_type <= 9)
    {
        slice_type %= 5;
    }
    switch (slice_type)
    {
        case static_cast<uint8_t>(H264SliceType::P):
            return 0b10;
        case static_cast<uint8_t>(H264SliceType::I):
            return 0b11;
        default:
        case static_cast<uint8_t>(H264SliceType::B):
        case static_cast<uint8_t>(H264SliceType::Sp):
        case static_cast<uint8_t>(H264SliceType::Si):
            return -1;
    }
}