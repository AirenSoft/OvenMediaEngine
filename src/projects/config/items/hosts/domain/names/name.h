//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	struct Name : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetName, _name)

	protected:
		void MakeParseList() override
		{
			RegisterValue<ValueType::Text>(nullptr, &_name);
		}

		ov::String _name;
	};
}  // namespace cfg