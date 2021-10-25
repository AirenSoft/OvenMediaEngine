#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <cstdint>

// If this enumeration is updated, also update IsKnownH264NalUnitType
enum class H264NalUnitType : uint8_t
{
    Unspecified = 0,
    NonIdrSlice = 1,
    DPA = 2, // CodedSliceDataPartitionA
    DPB = 3, // CodedSliceDataPartitionB
    DPC = 4, // CodedSliceDataPartitionC
    IdrSlice = 5,
    Sei = 6,
    Sps = 7,
    Pps = 8,
    Aud = 9,
    EndOfSequence = 10,
    EndOfStream = 11,
    FillerData = 12,
    SpsExt = 13,
    AuxiliarySlice = 19
};

ov::String NalUnitTypeToStr(uint8_t nal_unit_type);
bool IsValidH264NalUnitType(uint8_t nal_unit_type);
bool IsKnownH264NalUnitType(uint8_t nal_unit_type);

bool operator==(uint8_t first, H264NalUnitType second);
bool operator!=(uint8_t first, H264NalUnitType second);
