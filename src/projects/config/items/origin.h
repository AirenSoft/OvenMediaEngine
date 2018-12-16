//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	struct Origin : public Item
	{
		ov::String GetPrimary() const
		{
			return _primary;
		}

		ov::String GetSecondary() const
		{
			return _secondary;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Primary", &_primary);
			RegisterValue<Optional>("Secondary", &_secondary);
		}

		ov::String _primary;
		ov::String _secondary;
	};
}