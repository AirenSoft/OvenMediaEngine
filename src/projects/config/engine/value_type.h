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

#include <any>
#include <string>
#include <vector>

#include "config_error.h"

namespace cfg
{
	class Attribute;
	class Text;
	class Item;

	// #region ========== ValueType ==========
	enum class ValueType : uint32_t
	{
		// Unknown type
		Unknown,
		// A text value
		String,
		// A 32-bits integer value
		Integer,
		// A 64-bits integer value
		Long,
		// A boolean value
		Boolean,
		// A double value
		Double,
		// An attribute
		Attribute,
		// A text of itself
		Text,
		// An item
		Item,
		// A list value
		List
	};
	// #endregion

	const char *StringFromValueType(ValueType type);

	// Probe a type of Tprobe
	template <typename Tprobe, typename Tdummy = void>
	struct ProbeType
	{
		static constexpr ValueType type = ValueType::Unknown;
		static constexpr ValueType sub_type = ValueType::Unknown;
	};

	template <>
	struct ProbeType<ov::String>
	{
		static constexpr ValueType type = ValueType::String;
		static constexpr ValueType sub_type = ValueType::Unknown;
	};

	template <>
	struct ProbeType<int32_t>
	{
		static constexpr ValueType type = ValueType::Integer;
		static constexpr ValueType sub_type = ValueType::Unknown;
	};

	template <>
	struct ProbeType<int64_t>
	{
		static constexpr ValueType type = ValueType::Long;
		static constexpr ValueType sub_type = ValueType::Unknown;
	};

	template <>
	struct ProbeType<bool>
	{
		static constexpr ValueType type = ValueType::Boolean;
		static constexpr ValueType sub_type = ValueType::Unknown;
	};

	template <>
	struct ProbeType<double>
	{
		static constexpr ValueType type = ValueType::Double;
		static constexpr ValueType sub_type = ValueType::Unknown;
	};

	template <>
	struct ProbeType<Attribute>
	{
		static constexpr ValueType type = ValueType::Attribute;
		static constexpr ValueType sub_type = ValueType::Unknown;
	};

	template <typename Tprobe>
	struct ProbeType<Tprobe, std::enable_if_t<std::is_base_of_v<Text, Tprobe>>>
	{
		static constexpr ValueType type = ValueType::Text;
		static constexpr ValueType sub_type = ValueType::Unknown;
	};

	// Tprobe instance of Item
	template <typename Tprobe>
	struct ProbeType<Tprobe, std::enable_if_t<std::is_base_of_v<Item, Tprobe>>>
	{
		static constexpr ValueType type = ValueType::Item;
		static constexpr ValueType sub_type = ValueType::Unknown;
	};

	template <typename Titem>
	struct ProbeType<std::vector<Titem>>
	{
		static constexpr ValueType type = ValueType::List;
		static constexpr ValueType sub_type = ProbeType<Titem>::type;
	};

	ov::String MakeIndentString(int indent_count);

	ov::String ToString(const int *value);
	ov::String ToString(const int64_t *value);
	ov::String ToString(const float *value);
	ov::String ToString(const double *value);
	ov::String ToString(const ov::String *value);
	ov::String ToString(int indent_count, const Item *object);

	struct CastException
	{
		CastException(ov::String from, ov::String to)
			: from(from),
			  to(to)
		{
		}

		ov::String from;
		ov::String to;
	};

	template <typename Toutput_type>
	Toutput_type TryCast([[maybe_unused]] const std::any &value)
	{
		try
		{
			return std::any_cast<Toutput_type>(value);
		}
		catch (const std::bad_any_cast &)
		{
			throw CastException(
				ov::Demangle(value.type().name()).CStr(),
				ov::Demangle(typeid(Toutput_type).name()).CStr());
		}
	}

	template <typename Toutput_type, typename Ttype, typename... Tcandidates>
	Toutput_type TryCast(const std::any &value)
	{
		try
		{
			return std::any_cast<Ttype>(value);
		}
		catch (const std::bad_any_cast &)
		{
		}

		return TryCast<Toutput_type, Tcandidates...>(value);
	}
}  // namespace cfg
