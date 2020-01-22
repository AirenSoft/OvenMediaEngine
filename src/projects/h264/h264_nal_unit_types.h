#pragma once

#include <cstdint>

// If this enumeration is updated, also update IsKnownH264NalUnitType
enum class H264NalUnitType : uint8_t
{
    NonIdrSlice = 1,
    IdrSlice = 5,
    Sei = 6,
    Sps = 7,
    Pps = 8,
    Aud = 9
};

bool IsValidH264NalUnitType(uint8_t nal_unit_type);
bool IsKnownH264NalUnitType(uint8_t nal_unit_type);

bool operator==(uint8_t first, H264NalUnitType second);
bool operator!=(uint8_t first, H264NalUnitType second);
