//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "ice_server.h"

namespace cfg
{
	namespace bind
	{
		namespace cmm
		{
			struct IceServers : public Item
			{
			protected:
				std::vector<IceServer> _ice_server_list;

			public:
				CFG_DECLARE_REF_GETTER_OF(GetIceServerList, _ice_server_list);

			protected:
				void MakeList() override
				{
					Register("IceServer", &_ice_server_list);
				}
			};
		}  // namespace pub
	}	   // namespace bind
}  // namespace cfg