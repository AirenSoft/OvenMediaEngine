// I developed the code of this file by referring to the source code of virinext's hevcsbrowser (https://github.com/virinext/hevcesbrowser). 
// Thanks to virinext.
// - Getroot
#pragma once

#include <base/ovlibrary/ovlibrary.h>

enum class H265NALUnitType : uint8_t
{
	TRAIL_N    = 0,
	TRAIL_R    = 1,
	TSA_N      = 2,
	TSA_R      = 3,
	STSA_N     = 4,
	STSA_R     = 5,
	RADL_N     = 6,
	RADL_R     = 7,
	RASL_N     = 8,
	RASL_R     = 9,
	BLA_W_LP   = 16,
	BLA_W_RADL = 17,
	BLA_N_LP   = 18,
	IDR_W_RADL = 19,
	IDR_N_LP   = 20,
	CRA_NUT    = 21,
	IRAP_VCL22 = 22,
	IRAP_VCL23 = 23,
	VPS        = 32,
	SPS        = 33,
	PPS        = 34,
	AUD        = 35,
	EOS_NUT    = 36,
	EOB_NUT    = 37,
	FD_NUT     = 38,
	SEI_PREFIX = 39,
	SEI_SUFFIX = 40,
};
constexpr const char *EnumToString(H265NALUnitType nal_unit_type)
{
	switch (nal_unit_type)
	{
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, TRAIL_N);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, TRAIL_R);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, TSA_N);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, TSA_R);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, STSA_N);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, STSA_R);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, RADL_N);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, RADL_R);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, RASL_N);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, RASL_R);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, BLA_W_LP);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, BLA_W_RADL);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, BLA_N_LP);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, IDR_W_RADL);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, IDR_N_LP);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, CRA_NUT);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, IRAP_VCL22);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, IRAP_VCL23);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, VPS);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, SPS);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, PPS);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, AUD);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, EOS_NUT);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, EOB_NUT);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, FD_NUT);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, SEI_PREFIX);
		OV_CASE_RETURN_ENUM_STRING(H265NALUnitType, SEI_SUFFIX);
	}

	return "(Unknown)";
}

bool operator==(uint8_t first, H265NALUnitType second);
bool operator!=(uint8_t first, H265NALUnitType second);
