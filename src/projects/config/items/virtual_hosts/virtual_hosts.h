//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "virtual_host.h"

namespace cfg
{
	namespace vhost
	{
		struct VirtualHosts : public Item
		{
			CFG_DECLARE_REF_GETTER_OF(GetVirtualHostList, _virtual_host_list)

		protected:
			void MakeParseList() override
			{
				RegisterValue("VirtualHost", &_virtual_host_list);
			}

			std::vector<VirtualHost> _virtual_host_list;
		};
	}  // namespace vhost
}  // namespace cfg
