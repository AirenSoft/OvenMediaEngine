//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "host.h"

namespace cfg
{
	struct Hosts : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional, Includable>("Host", &_host);

			return result;
		}

	protected:
		Value <std::vector<Host>> _host;
	};
}