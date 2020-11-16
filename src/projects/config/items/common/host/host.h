//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "names.h"
#include "tls.h"

namespace cfg
{
	namespace cmn
	{
		struct Host : public Item
		{
			CFG_DECLARE_REF_GETTER_OF(GetNameList, _names.GetNameList())
			CFG_DECLARE_REF_GETTER_OF(GetNames, _names)
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
	}  // namespace cmn
}  // namespace cfg