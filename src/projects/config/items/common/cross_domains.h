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
	namespace cmn
	{
		struct CrossDomains : public Item
		{
		protected:
			std::vector<ov::String> _url_list;

		public:
			CFG_DECLARE_REF_GETTER_OF(GetUrls, _url_list)

		protected:
			void MakeList() override
			{
				Register("Url", &_url_list);
			}
		};
	}  // namespace cmn
}  // namespace cfg