#pragma once

#include <cstdint>

enum class H264SliceType : uint8_t
{
    P = 0,
    B = 1,
    I = 2,
    Sp = 3,
    Si = 4
};

int16_t GetSliceTypeNalRefIdc(uint8_t slice_type);