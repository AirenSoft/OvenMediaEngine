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
	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional, Includable>("Stream", &_stream_list);
		}

		std::vector<Stream> _stream_list;
	};
}