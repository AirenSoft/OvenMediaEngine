#include "h264_nal_unit_types.h"

#include <base/ovlibrary/log.h>

#define OV_LOG_TAG "H264NalUnitTypes"

bool IsValidH264NalUnitType(uint8_t nal_unit_type)
{
#if defined(DEBUG)
    IsKnownH264NalUnitType(nal_unit_type);
#endif
    return (0 < nal_unit_type && nal_unit_type < 16) || (18 < nal_unit_type && nal_unit_type < 21);
}

bool IsKnownH264NalUnitType(uint8_t nal_unit_type)
{
    switch (nal_unit_type)
    {
    case static_cast<uint8_t>(H264NalUnitType::NonIdrSlice):
    case static_cast<uint8_t>(H264NalUnitType::IdrSlice):
    case static_cast<uint8_t>(H264NalUnitType::Sei):
    case static_cast<uint8_t>(H264NalUnitType::Sps):
    case static_cast<uint8_t>(H264NalUnitType::Pps):
    case static_cast<uint8_t>(H264NalUnitType::Aud):
        return true;
    default:
        logtw("Unsupported NAL unit type %u", nal_unit_type);
        return false;
    }
}

bool operator==(uint8_t first, H264NalUnitType second)
{
    return first == static_cast<uint8_t>(second);
}

bool operator!=(uint8_t first, H264NalUnitType second)
{
    return first != static_cast<uint8_t>(second);
}
