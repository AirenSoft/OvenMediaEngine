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