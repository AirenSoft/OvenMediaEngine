//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	struct Port : public Item
	{
		Port(int port)
			: _port(ov::Converter::ToString(port))
		{
		}

		int GetPort() const
		{
			return ov::Converter::ToInt32(_port);
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<ValueType::Text>(nullptr, &_port);
		}

		ov::String _port;
	};
}