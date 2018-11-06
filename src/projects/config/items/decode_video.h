//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	struct DecodeVideo : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue("HWAcceleration", &_hw_acceleration);

			return result;
		}

	protected:
		Value <ov::String> _hw_acceleration;
	};
}