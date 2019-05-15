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
		const std::vector<Host> &GetHosts() const
		{
			return _hosts.GetHosts();
		}
        const ov::String &GetVersion() const
        {
            return _version;
        }

	protected:
		void MakeParseList() const override
		{
			RegisterValue<ValueType::Attribute>("version", &_version);
			RegisterValue<Optional>("Name", &_name);
			RegisterValue<Optional>("Hosts", &_hosts);
		}

		ov::String _version = "1.0";
		ov::String _name;

		Hosts _hosts;
	};
}