//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	struct Pass : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetScheme, _scheme)
		CFG_DECLARE_REF_GETTER_OF(GetUrlList, _url_list)

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("Scheme", &_scheme);
			RegisterValue("Url", &_url_list);
		}

		ov::String _scheme;
		std::vector<Url> _url_list;
	};
}  // namespace cfg