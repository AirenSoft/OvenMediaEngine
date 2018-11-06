//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"
#include "tls.h"

namespace cfg
{
	struct HlsPublisher : public Publisher
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("Port", &_port);
			result = result && RegisterValue<Optional>("TLS", &_tls);

			return result;
		}

	protected:
		Value<int> _port = 80;
		Value <Tls> _tls;
	};
}