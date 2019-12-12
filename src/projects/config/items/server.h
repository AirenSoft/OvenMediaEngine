//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "hosts/hosts.h"

namespace cfg
{
	struct Server : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetVersion, _version)
		CFG_DECLARE_REF_GETTER_OF(GetName, _name)

		CFG_DECLARE_REF_GETTER_OF(GetHostList, _hosts.GetHostList())

	protected:
		void MakeParseList() override
		{
			RegisterValue<ValueType::Attribute>("version", &_version);
			RegisterValue<Optional>("Name", &_name);

			RegisterValue<Optional>("Hosts", &_hosts);
		}

		ov::String _version;
		ov::String _name;

		Hosts _hosts;
	};
}  // namespace cfg