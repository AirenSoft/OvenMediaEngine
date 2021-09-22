//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

namespace cfg
{
	class Item;

	class Attribute
	{
	protected:
		friend class Item;

	public:
		const ov::String &GetValue() const
		{
			return _value;
		}

		operator const char *() const
		{
			return _value;
		}

	protected:
		ov::String _value;
	};
}  // namespace cfg
