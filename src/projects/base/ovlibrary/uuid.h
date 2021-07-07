//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./ovlibrary.h"
#include "./string.h"

#include <uuid/uuid.h>

namespace ov
{
	class UUID
	{
	public:
		static String Generate()
		{
			std::string res;
			uuid_t uuid;

			char uuid_cstr[37]; // 36 bytes uuid
			uuid_generate(uuid);
			uuid_unparse(uuid, uuid_cstr);
			res = std::string(uuid_cstr);

	        return res.c_str();
		}
	};
}