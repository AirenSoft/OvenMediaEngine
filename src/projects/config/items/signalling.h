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
		int GetListenPort() const
		{
			return _listen_port;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("ListenPort", &_listen_port);
		}

		int _listen_port = 3333;
	};
}