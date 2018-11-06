//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stream.h"

namespace cfg
{
	struct Streams : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional, Includable>("Stream", &_stream_list);

			return result;
		}

	protected:
		Value<std::vector<Stream>> _stream_list;
	};
}