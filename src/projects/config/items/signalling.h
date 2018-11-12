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
	struct Signalling : public Item
	{
		int GetPort() const
		{
			return _port;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Port", &_port);
		}

		int _port = 3333;
	};
}