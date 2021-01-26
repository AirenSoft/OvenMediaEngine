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
	namespace vhost
	{
		namespace orgn
		{
			struct Origins : public Item
			{
				CFG_DECLARE_REF_GETTER_OF(GetOriginList, _origin_list)

			protected:
				void MakeList() override
				{
					Register<Optional>("Origin", &_origin_list);
				}

				std::vector<Origin> _origin_list;
			};
		}  // namespace orgn
	}	   // namespace vhost
}  // namespace cfg