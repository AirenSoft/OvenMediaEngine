//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "urls/urls.h"

namespace cfg
{
	struct Pass : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetScheme, _scheme)
		CFG_DECLARE_REF_GETTER_OF(GetUrlList, _urls.GetUrlList())
		CFG_DECLARE_REF_GETTER_OF(GetUrls, _urls)

	protected:
		void MakeParseList() override
		{
			RegisterValue("Scheme", &_scheme);
			RegisterValue("Urls", &_urls);
		}

		ov::String _scheme;
		Urls _urls;
	};
}  // namespace cfg