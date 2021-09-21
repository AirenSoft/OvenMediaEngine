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

#include "config_error.h"

namespace cfg
{
	class Item;

	class Text
	{
	protected:
		friend class Item;

	public:
		virtual ov::String ToString() const = 0;

	protected:
		MAY_THROWS(std::shared_ptr<ConfigError>)
		virtual void FromString(const ov::String &str) = 0;

		bool IsParsed(const void *target) const
		{
			throw CreateConfigError("Use Item::IsParsed() instead");
		}
	};
}  // namespace cfg
