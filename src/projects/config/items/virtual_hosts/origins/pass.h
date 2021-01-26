//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../common/urls.h"

namespace cfg
{
	namespace vhost
	{
		namespace orgn
		{
			struct Pass : public Item
			{
				CFG_DECLARE_REF_GETTER_OF(GetScheme, _scheme)
				CFG_DECLARE_REF_GETTER_OF(GetUrlList, _urls.GetUrlList())
				CFG_DECLARE_REF_GETTER_OF(GetUrls, _urls)

			protected:
				void MakeList() override
				{
					Register("Scheme", &_scheme);
					Register("Urls", &_urls);
				}

				ov::String _scheme;
				cmn::Urls _urls;
			};
		}  // namespace orgn
	}	   // namespace vhost
}  // namespace cfg