//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

class H265Converter
{
public:
	static std::shared_ptr<const ov::Data> ConvertAnnexbToLengthPrefixed(const std::shared_ptr<const ov::Data> &data);
};
