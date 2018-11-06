//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "encode.h"

namespace cfg
{
	struct Encodes : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional, Includable>("Encode", &_encode_list);

			return result;
		}

	protected:
		Value<std::vector<Encode>> _encode_list;
	};
}