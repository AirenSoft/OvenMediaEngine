//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "encode.h"

namespace cfg
{
	struct Encodes : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetEncodeList, _encode_list);

	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("Encode", &_encode_list);
		}

		std::vector<Encode> _encode_list;
	};
}  // namespace cfg