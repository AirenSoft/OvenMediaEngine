//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "host.h"

namespace cfg
{
	struct Hosts : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetHostList, _host_list)

	protected:
		void MakeParseList() override
		{
			RegisterValue("Host", &_host_list);
		}

		std::vector<Host> _host_list;
	};
}  // namespace cfg
