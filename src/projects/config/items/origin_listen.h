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
	struct OriginListen : public Item
	{
		ov::String GetIp() const
		{
			return _ip;
		}

		int GetPort() const
		{
			return _port;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("IP", &_ip);
			RegisterValue<Optional>("Port", &_port);
		}

		ov::String _ip = "*";
		int _port = 9000;
	};
}