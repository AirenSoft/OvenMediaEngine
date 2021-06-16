//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <type_traits>

namespace ov
{
	template <typename T>
	std::underlying_type_t<T> ToUnderlyingType(T enum_value)
	{
		return static_cast<std::underlying_type_t<T>>(enum_value);
	}
}  // namespace ov
