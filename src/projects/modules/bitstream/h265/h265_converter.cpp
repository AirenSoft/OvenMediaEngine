//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "h265_converter.h"

#include "../h264/h264_converter.h"

#define OV_LOG_TAG "H265Converter"

std::shared_ptr<const ov::Data> H265Converter::ConvertAnnexbToLengthPrefixed(const std::shared_ptr<const ov::Data> &data)
{
	// Same as H.264
	return H264Converter::ConvertAnnexbToAvcc(data);
}