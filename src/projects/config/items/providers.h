//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtmp_provider.h"

namespace cfg
{
	struct Providers : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("RTMP", &_rtmp_provider);

			return result;
		}

	protected:
		Value<RtmpProvider> _rtmp_provider;
	};
}