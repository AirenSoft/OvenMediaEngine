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
		const Tls &GetTls() const
		{
			return _tls;
		}

		int GetPort() const
		{
			return _port;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("TLS", &_tls);
			RegisterValue<Optional>("Port", &_port);
		}

		Tls _tls;
		int _port = 3333;
	};
}