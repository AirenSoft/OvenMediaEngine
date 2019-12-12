//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "url.h"

namespace cfg
{
	struct CrossDomain : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetUrls, _url_list)

	protected:
		void MakeParseList() override
		{
			RegisterValue("Url", &_url_list);
		}

		std::vector<Url> _url_list;
	};
}  // namespace cfg