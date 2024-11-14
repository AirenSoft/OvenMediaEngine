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
	inline std::underlying_type_t<T> ToUnderlyingType(T enum_value)
	{
		return static_cast<std::underlying_type_t<T>>(enum_value);
	}

	template <typename T>
	using UnderylingType = std::underlying_type_t<T>;
}  // namespace ov

#define OV_DEFINE_SETTER(type, setter, member, extra_qualifier) \
	void setter(const type &value) extra_qualifier              \
	{                                                           \
		member = value;                                         \
	}

#define OV_DEFINE_GETTER(type, getter, member, extra_qualifier) \
	type getter() extra_qualifier                               \
	{                                                           \
		return member;                                          \
	}

#define OV_DEFINE_CONST_GETTER(type, getter, member, extra_qualifier) \
	const type &getter() const extra_qualifier                        \
	{                                                                 \
		return member;                                                \
	}

#define OV_DEFINE_SETTER_CONST_GETTER(type, setter, getter, member, extra_qualifier) \
	OV_DEFINE_SETTER(type, setter, member, extra_qualifier)                          \
	OV_DEFINE_CONST_GETTER(type, getter, member, extra_qualifier)
