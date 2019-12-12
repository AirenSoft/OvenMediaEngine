//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "names/names.h"
#include "tls.h"

namespace cfg
{
	struct Domain : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetNameList, _names.GetNameList())
		CFG_DECLARE_REF_GETTER_OF(GetTls, _tls)

	protected:
		void MakeParseList() override
		{
			RegisterValue("Names", &_names);
			RegisterValue<Optional>("TLS", &_tls);
		}

		Names _names;
		Tls _tls;
	};
}  // namespace cfg