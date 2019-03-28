//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "host.h"

namespace cfg
{
	struct Hosts : public Item
	{
		const std::vector<Host> &GetHosts() const
		{
			return _host_list;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Host", &_host_list);
		}

		std::vector<Host> _host_list;
	};
}