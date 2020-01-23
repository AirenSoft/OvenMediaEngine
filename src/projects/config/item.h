//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "value.h"

#include <utility>

// virtual int GetInt() const { ... }
#define CFG_DECLARE_VIRTUAL_GETTER_OF(type, function_name, variable_name) \
	virtual type function_name() const                                    \
	{                                                                     \
		return variable_name;                                             \
	}

// int GetInt() const { ... }
#define CFG_DECLARE_GETTER_OF(function_name, variable_name) \
	auto function_name() const                              \
	{                                                       \
		return variable_name;                               \
	}

// const Object &GetObject() const { ... }
#define CFG_DECLARE_REF_GETTER_OF(function_name, variable_name) \
	const auto &function_name() const                           \
	{                                                           \
		return variable_name;                                   \
	}

// int GetInt() const override { ... }
#define CFG_DECLARE_OVERRIDED_GETTER_OF(type, function_name, variable_name) \
	type function_name() const override                                     \
	{                                                                       \
		return variable_name;                                               \
	}

namespace cfg
{
	//region Annotations

	struct Optional;
	struct CondOptional;
	struct ResolvePath;

	//endregion

	struct Item
	{
		Item() = default;
		Item(const Item &item);
		Item(Item &&item) noexcept;
		virtual ~Item() = default;

		Item &operator=(const Item &item);

		bool Parse(const ov::String &file_name, const ov::String &tag_name);
		bool IsParsed() const;

		bool IsDefault(const void *target) const;

		const Item *GetParent() const
		{
			return _parent;
		}

		const Item *GetParent(const ov::String &name) const;

		template <class T>
		const T *GetParentAs(const ov::String &name) const
		{
			auto parent = GetParent(name);

			return dynamic_cast<const T *>(parent);
		}

		virtual ov::String ToString() const
		{
			return std::move(ToString(false));
		}

		virtual ov::String ToString(bool exclude_default) const;

	protected:
		enum class ParseResult
		{
			// Item was parsed successfully
			Parsed,

			// Item not found, and it is optional
			NotParsed,

			// Item not found, and it is mandatory
			Error
		};

		const ov::String GetTagName() const;

		//region ========== Annotation Utilities ==========

		template <typename... Args>
		struct CheckAnnotations;

		template <typename Texpected, typename Ttype, typename... Ttypes>
		struct CheckAnnotations<Texpected, Ttype, Ttypes...>
		{
			static constexpr bool value = std::is_same<Texpected, Ttype>::value || CheckAnnotations<Texpected, Ttypes...>::value;
		};

		template <typename Texpected>
		struct CheckAnnotations<Texpected>
		{
			static constexpr bool value = false;
		};

		//endregion

		//region ========== ProbeType ==========

		static constexpr ValueType ProbeType(const int *target)
		{
			return ValueType::Integer;
		}

		static constexpr ValueType ProbeType(const bool *target)
		{
			return ValueType::Boolean;
		}

		static constexpr ValueType ProbeType(const float *target)
		{
			return ValueType::Float;
		}

		static constexpr ValueType ProbeType(const ov::String *target)
		{
			return ValueType::String;
		}

		static constexpr ValueType ProbeType(const Item *target)
		{
			return ValueType::Element;
		}

		//endregion

		//region ========== ParseItem ==========

		struct ParseItem
		{
			ParseItem() = default;

			ParseItem(ov::String name, bool is_parsed, bool from_default, ValueBase *value)
				: name(std::move(name)),
				  is_parsed(is_parsed),
				  from_default(from_default),
				  value(value)
			{
			}

			ParseItem(ov::String name, ValueBase *value)
				: ParseItem(std::move(name), false, false, value)
			{
			}

			ov::String name;
			bool is_parsed = false;
			bool from_default = true;

			std::shared_ptr<ValueBase> value;
		};

		//endregion

		//region ========== RegisterValue ==========

		void Register(const ov::String &name, ValueBase *value, bool is_optional, bool is_conditional_optional, bool need_to_resolve_path, ValueBase::OptionalCallback conditional_optional_callback, ValueBase::ValidationCallback validation_callback = nullptr);

		// For int, bool, float, ov::String with value_type
		template <
			ValueType value_type,
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void,
			typename Ttype>
		void RegisterValue(const ov::String &name, const Ttype *target, ValueBase::OptionalCallback optional_callback = nullptr, ValueBase::ValidationCallback validation_callback = nullptr, ValueType type = value_type)
		{
			Register(
				name, new Value<Ttype>(type, const_cast<Ttype *>(target)),
				CheckAnnotations<Optional, Tannotation1, Tannotation2, Tannotation3>::value,
				CheckAnnotations<CondOptional, Tannotation1, Tannotation2, Tannotation3>::value,
				CheckAnnotations<ResolvePath, Tannotation1, Tannotation2, Tannotation3>::value,
				optional_callback, validation_callback);
		};

		// For int, bool, float, ov::String without value_type (use ProbeType())
		template <
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void,
			typename Ttype, typename std::enable_if<std::is_base_of<Item, Ttype>::value == false>::type * = nullptr>
		void RegisterValue(const ov::String &name, const Ttype *target, ValueBase::OptionalCallback optional_callback = nullptr, ValueBase::ValidationCallback validation_callback = nullptr)
		{
			Register(
				name, new Value<Ttype>(ProbeType(target), const_cast<Ttype *>(target)),
				CheckAnnotations<Optional, Tannotation1, Tannotation2, Tannotation3>::value,
				CheckAnnotations<CondOptional, Tannotation1, Tannotation2, Tannotation3>::value,
				CheckAnnotations<ResolvePath, Tannotation1, Tannotation2, Tannotation3>::value,
				optional_callback, validation_callback);
		}

		// For subclass of Item
		template <
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void,
			typename Ttype, typename std::enable_if<std::is_base_of<Item, Ttype>::value>::type * = nullptr>
		void RegisterValue(const ov::String &name, const Ttype *target, ValueBase::OptionalCallback optional_callback = nullptr, ValueBase::ValidationCallback validation_callback = nullptr)
		{
			Register(
				name, new ValueForElement<Ttype>(const_cast<Ttype *>(target)),
				CheckAnnotations<Optional, Tannotation1, Tannotation2, Tannotation3>::value,
				CheckAnnotations<CondOptional, Tannotation1, Tannotation2, Tannotation3>::value,
				CheckAnnotations<ResolvePath, Tannotation1, Tannotation2, Tannotation3>::value,
				optional_callback, validation_callback);
		}

		// For list of Items
		template <
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void,
			typename Ttype>
		void RegisterValue(const ov::String &name, const std::vector<Ttype> *target, ValueBase::OptionalCallback optional_callback = nullptr, ValueBase::ValidationCallback validation_callback = nullptr)
		{
			Register(
				name, new ValueForList<Ttype>(const_cast<std::vector<Ttype> *>(target)),
				CheckAnnotations<Optional, Tannotation1, Tannotation2, Tannotation3>::value,
				CheckAnnotations<CondOptional, Tannotation1, Tannotation2, Tannotation3>::value,
				CheckAnnotations<ResolvePath, Tannotation1, Tannotation2, Tannotation3>::value,
				optional_callback, validation_callback);
		}

		//endregion

		void MakeParseList() const
		{
			// This is a safe operation because _parse_list is intended for keeping items up to date at all times
			(const_cast<Item *>(this))->MakeParseList();
		}

		virtual void MakeParseList() = 0;

		ValueBase *FindValue(const ov::String &name, const ParseItem *parse_item_to_find);
		// target은 하위 항목
		bool IsParsed(const void *target) const;

		Item::ParseResult ParseFromFile(const ov::String &base_file_name, ov::String file_name, const ov::String &tag_name, int indent);
		// node는 this 레벨에 준하는 항목임. 즉, node.name() == _tag_name.CStr() 관계가 성립
		virtual Item::ParseResult ParseFromNode(const ov::String &base_file_name, const pugi::xml_node &node, const ov::String &tag_name, int indent);

		static ov::String GetEnv(const char *key, const char *default_value, bool *is_default_value);
		ov::String Preprocess(const ov::String &xml_path, const ValueBase *value_base, const char *value, const ov::String &tag_name, int indent);

		ov::String GetSelector(const pugi::xml_node &node);

		virtual ov::String ToString(int indent, bool exclude_default) const;
		ov::String ToString(const ParseItem *parse_item, int indent, bool exclude_default, bool append_new_line) const;

		ov::String _tag_name;
		std::vector<ParseItem> _parse_list;

		bool _parsed = false;

		Item *_parent = nullptr;
	};
};  // namespace cfg