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
	struct WebrtcPublisher : public Publisher
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue<Optional>("Port", &_port);
			result = result && RegisterValue<Optional>("TLS", &_tls);

			return result;
		}

	protected:
		Value <ov::String> _port = "80/tcp";
		Value <Tls> _tls;
	};
}