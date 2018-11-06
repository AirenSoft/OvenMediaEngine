//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "application.h"

namespace cfg
{
	struct Applications : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional, Includable>("Application", &_application_list);

			return result;
		}

	protected:
		Value <std::vector<Application>> _application_list;
	};
}