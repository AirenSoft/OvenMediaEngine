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

#define OV_DEFINE_SETTER(value_type, setter_return, setter, member, extra_qualifier, pre_process, post_process) \
	setter_return setter(const value_type &value) extra_qualifier                                               \
	{                                                                                                           \
		pre_process;                                                                                            \
		member = value;                                                                                         \
		post_process;                                                                                           \
	}

#define OV_DEFINE_GETTER(value_type, getter, member, extra_qualifier) \
	value_type getter() extra_qualifier                               \
	{                                                                 \
		return member;                                                \
	}

#define OV_DEFINE_CONST_GETTER(value_type, getter, member, extra_qualifier) \
	const value_type &getter() const extra_qualifier                        \
	{                                                                       \
		return member;                                                      \
	}

#define OV_DEFINE_SETTER_CONST_GETTER(value_type, setter_return, setter, getter, member, extra_qualifier, pre_process, post_process) \
	OV_DEFINE_SETTER(value_type, setter_return, setter, member, extra_qualifier, pre_process, post_process)                          \
	OV_DEFINE_CONST_GETTER(value_type, getter, member, extra_qualifier)
