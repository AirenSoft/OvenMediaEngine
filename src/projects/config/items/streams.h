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
		const std::vector<Stream> &GetStreamList() const
		{
			return _stream_list;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Stream", &_stream_list);
		}

		std::vector<Stream> _stream_list;
	};
}