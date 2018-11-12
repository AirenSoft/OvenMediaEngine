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
	protected:
		void MakeParseList() const override
		{
			RegisterValue("HWAcceleration", &_hw_acceleration);
		}

		ov::String _hw_acceleration;
	};
}