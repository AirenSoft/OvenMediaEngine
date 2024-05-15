#include "h264_common.h"

#define OV_LOG_TAG "H264NalUnitTypes"

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
		case static_cast<uint8_t>(H264NalUnitType::DPA):
		case static_cast<uint8_t>(H264NalUnitType::DPB):
		case static_cast<uint8_t>(H264NalUnitType::DPC):
		case static_cast<uint8_t>(H264NalUnitType::IdrSlice):
		case static_cast<uint8_t>(H264NalUnitType::Sei):
		case static_cast<uint8_t>(H264NalUnitType::Sps):
		case static_cast<uint8_t>(H264NalUnitType::Pps):
		case static_cast<uint8_t>(H264NalUnitType::Aud):
		case static_cast<uint8_t>(H264NalUnitType::EndOfSequence):
		case static_cast<uint8_t>(H264NalUnitType::EndOfStream):
		case static_cast<uint8_t>(H264NalUnitType::FillerData):
		case static_cast<uint8_t>(H264NalUnitType::SpsExt):
		case static_cast<uint8_t>(H264NalUnitType::AuxiliarySlice):
			return true;
		default:
			logtw("Unsupported NAL unit type %u", nal_unit_type);
			return false;
	}
}

ov::String NalUnitTypeToStr(uint8_t nal_unit_type)
{
	switch (nal_unit_type)
	{
		case static_cast<uint8_t>(H264NalUnitType::NonIdrSlice):
			return "NonIdrSlice";
		case static_cast<uint8_t>(H264NalUnitType::DPA):
			return "DPA";
		case static_cast<uint8_t>(H264NalUnitType::DPB):
			return "DPB";
		case static_cast<uint8_t>(H264NalUnitType::DPC):
			return "DPC";
		case static_cast<uint8_t>(H264NalUnitType::IdrSlice):
			return "IdrSlice";
		case static_cast<uint8_t>(H264NalUnitType::Sei):
			return "Sei";
		case static_cast<uint8_t>(H264NalUnitType::Sps):
			return "Sps";
		case static_cast<uint8_t>(H264NalUnitType::Pps):
			return "Pps";
		case static_cast<uint8_t>(H264NalUnitType::Aud):
			return "Aud";
		case static_cast<uint8_t>(H264NalUnitType::EndOfSequence):
			return "EndOfSequence";
		case static_cast<uint8_t>(H264NalUnitType::EndOfStream):
			return "EndOfStream";
		case static_cast<uint8_t>(H264NalUnitType::FillerData):
			return "FillerData";
		case static_cast<uint8_t>(H264NalUnitType::SpsExt):
			return "SpsExt";
		case static_cast<uint8_t>(H264NalUnitType::AuxiliarySlice):
			return "AuxiliarySlice";
		default:
            break;
	}

    return "Unknown";
}

bool operator==(uint8_t first, H264NalUnitType second)
{
	return first == static_cast<uint8_t>(second);
}

bool operator!=(uint8_t first, H264NalUnitType second)
{
	return first != static_cast<uint8_t>(second);
}
