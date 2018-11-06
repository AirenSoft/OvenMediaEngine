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
	struct RtmpProvider : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("Port", &_port);

			return result;
		}

	protected:
		Value <ov::String> _port = "1935/tcp";
	};
}