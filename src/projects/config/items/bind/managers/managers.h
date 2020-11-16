//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "api.h"

namespace cfg
{
	namespace bind
	{
		namespace mgr
		{
			struct Managers : public Item
			{
				CFG_DECLARE_REF_GETTER_OF(GetApi, _api)

			protected:
				void MakeParseList() override
				{
					RegisterValue<Optional>("API", &_api);
				};

				API _api{"8081/tcp"};
			};
		}  // namespace mgr
	}	   // namespace bind
}  // namespace cfg