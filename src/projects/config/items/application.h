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
#include "decode.h"
#include "encodes.h"
#include "providers.h"
#include "publishers.h"

namespace cfg
{
	struct Application : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue("Name", &_name);
			result = result && RegisterValue<Optional>("Type", &_type);
			result = result && RegisterValue<Optional, Includable>("TLS", &_tls);
			result = result && RegisterValue<Optional, Includable>("Decode", &_decode);
			result = result && RegisterValue<Optional, Includable>("Encodes", &_encodes);
			result = result && RegisterValue<Optional, Includable>("Providers", &_providers);
			result = result && RegisterValue<Optional, Includable>("Publishers", &_publishers);

			return result;
		}

	protected:
		Value<ov::String> _name;
		Value<ov::String> _type;
		Value<Tls> _tls;
		Value<Decode> _decode;
		Value<Encodes> _encodes;
		Value<Providers> _providers;
		Value<Publishers> _publishers;
	};
}