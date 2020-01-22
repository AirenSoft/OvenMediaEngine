#pragma once

#include "h264_nal_unit_types.h"
#include "h264_slice_types.h"

constexpr auto kH264NalUnitTypeMask = 0b11111;
constexpr auto kH264NalRefIdcMask = 0b1100000;

#define GET_NAL_REF_IDC(x) (((x) & kH264NalRefIdcMask) >> 5)

constexpr auto kH264SpsPpsNalRefIdc = 0b11;