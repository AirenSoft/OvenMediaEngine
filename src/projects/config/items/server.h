//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "hosts.h"

namespace cfg
{
	struct Server : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<ValueType::Attribute>("version", &_version);
			result = result && RegisterValue<Optional>("Name", &_name);
			result = result && RegisterValue<Optional, Includable>("Hosts", &_hosts);

			return result;
		}

	protected:
		Value <ov::String> _version = "1.0";
		Value <ov::String> _name;

		Value <Hosts> _hosts;
	};
}