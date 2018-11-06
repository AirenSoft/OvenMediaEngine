//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "value_for_specialization.h"

namespace cfg
{
	// cfg::Item 특수화
	template<typename Ttype>
	class Value<Ttype, typename std::enable_if<std::is_base_of<Item, Ttype>::value>::type>
		: public ValueContainer<Item>
	{
	public:
		Value() : ValueContainer<Item>(Tconfig_type)
		{
			_value = std::shared_ptr<Item>(new Ttype());
		}

		Value(const Ttype &value) : Value() // NOLINT
		{
			auto new_value = new Ttype();
			*new_value = value;

			_value = std::shared_ptr<Item>(new_value);
		}

		void Reset() override
		{
			_value = std::shared_ptr<Item>(new Ttype());
		}

		bool ParseFromValue(const ValueBase *value, int indent) override
		{
			auto item = dynamic_cast<const Value<Item> *>(value);

			if(item == nullptr)
			{
				OV_ASSERT2(false);
				return false;
			}

			auto target = dynamic_cast<const Ttype *>(item);

			if(target == nullptr)
			{
				OV_ASSERT2(false);
				return false;
			}

			auto new_value = new Ttype();
			*new_value = *target;

			_value = std::shared_ptr<Item>(new_value);
			_is_parsed = true;

			return true;
		}

		bool ParseFromNode(const pugi::xml_node &node, bool processing_include_file, int indent) override
		{
			auto item = dynamic_cast<Ttype *>(_value.get());

			if(item == nullptr)
			{
				OV_ASSERT2(false);
				return false;
			}

			_is_parsed = ProcessItem(item, node, processing_include_file, indent + 1);

			return _is_parsed;
		}

		ov::String ToStringInternal(int indent, bool append_new_line) const override
		{
			auto value = dynamic_cast<const Ttype *>(_value.get());

			if(value == nullptr)
			{
				OV_ASSERT2(false);
				return "";
			}

			ov::String result = value->ToString(indent);

			if(append_new_line)
			{
				result.Append('\n');
			}

			return result;
		}

	protected:
		static constexpr ValueType Tconfig_type = ValueType::Element;
		using ValueContainer<Item>::_value;
		using ValueContainer<Item>::_is_parsed;
	};
}