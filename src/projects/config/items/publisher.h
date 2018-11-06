//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "streams.h"

namespace cfg
{
	struct Publisher : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("IP", &_ip);
			result = result && RegisterValue<Optional>("MaxConnection", &_max_connection);
			result = result && RegisterValue<Optional>("Streams", &_streams);

			return result;
		}

	protected:
		Value<ov::String> _ip;
		Value<int> _max_connection = 0;
		Value<Streams> _streams;
	};
}