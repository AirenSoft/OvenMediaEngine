//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "origin.h"

namespace cfg
{
	struct Origins : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetOriginList, _origin_list)

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("Origin", &_origin_list);
		}

		std::vector<OriginsOrigin> _origin_list;
	};
}  // namespace cfg