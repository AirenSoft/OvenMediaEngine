//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "tls.h"

namespace cfg
{
	struct Signalling : public Item
	{
		const Tls &GetTls() const
		{
			return _tls;
		}

		int GetListenPort() const
		{
			return _listen_port;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("TLS", &_tls);
			RegisterValue<Optional>("ListenPort", &_listen_port);
		}

		Tls _tls;
		int _listen_port = 3333;
	};
}