//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	struct Url : public Item
	{
		ov::String GetUrl() const
		{
			return _url;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<ValueType::Text>(nullptr, &_url);
		}

		ov::String _url;
	};
}