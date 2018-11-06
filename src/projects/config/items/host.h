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
#include "providers.h"
#include "publishers.h"
#include "applications.h"

namespace cfg
{
	struct Host : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue("Name", &_name);
			result = result && RegisterValue<Optional>("IP", &_ip);
			result = result && RegisterValue<Optional, Includable>("TLS", &_tls);
			result = result && RegisterValue<Optional, Includable>("Providers", &_providers);
			result = result && RegisterValue<Optional, Includable>("Publishers", &_publishers);
			result = result && RegisterValue<Optional, Includable>("Applications", &_applications);

			return result;
		}

	protected:
		Value <ov::String> _name;
		Value <ov::String> _ip;
		Value <Tls> _tls;
		Value <Providers> _providers;
		Value <Publishers> _publishers;
		Value <Applications> _applications;
	};
}