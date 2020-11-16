//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "name.h"

namespace cfg
{
	namespace cmn
	{
		struct Names : public Item
		{
			CFG_DECLARE_REF_GETTER_OF(GetNameList, _name_list);

		protected:
			void MakeParseList() override
			{
				RegisterValue("Name", &_name_list);
			}

			std::vector<Name> _name_list;
		};
	}  // namespace cmn
}  // namespace cfg