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
	// std::vector<> 특수화
	template<typename Ttype>
	class Value<std::vector<Ttype>, typename std::enable_if<std::is_base_of<Item, Ttype>::value>::type>
		: public ValueContainer<std::vector<std::shared_ptr<Item>>>
	{
	protected:
		using Tvector = std::vector<std::shared_ptr<Item>>;
	public:
		Value() : ValueContainer<Tvector>(Tconfig_type)
		{
			_value = std::make_shared<Tvector>();
		}

		Value(const Ttype &value) : Value() // NOLINT
		{
			_value = std::make_shared<Tvector>(value);
		}

		void Reset() override
		{
			_value = std::make_shared<Tvector>();
		}

		bool ParseFromValue(const ValueBase *value, int indent) override
		{
			auto from_value = dynamic_cast<const Value<Tvector> *>(value);

			if(from_value == nullptr)
			{
				return false;
			}

			const Tvector *list = *from_value;
			Tvector *target = _value.get();

			target->clear();

			for(auto &item : *list)
			{
				auto instance = dynamic_cast<const Ttype *>(item.get());

				if((item != nullptr) && (instance == nullptr))
				{
					// conversion failed
					return false;
				}

				target->push_back(item);
			}

			_is_parsed = true;

			return true;
		}

		bool ParseFromAttribute(const pugi::xml_attribute &attribute, bool processing_include_file, int indent) override
		{
			return false;
		}

		bool ParseFromNode(const pugi::xml_node &node, bool processing_include_file, int indent) override
		{
			auto list = _value.get();

			auto instance = new Ttype();
			auto item = std::shared_ptr<Item>(instance);

			if(ProcessItem(item.get(), node, processing_include_file, indent + 1))
			{
				instance->SetParent(GetParent());
				list->push_back(std::move(item));

				_is_parsed = true;
				return true;
			}

			return false;
		}

		ov::String ToStringInternal(int indent, bool append_new_line) const override
		{
			ov::String indent_string = ValueBase::MakeIndentString(indent + 1);
			auto list = _value.get();

			if(list->empty())
			{
				return "(no items) []";
			}

			ov::String result = ov::String::FormatString("(%d items) [", list->size());
			bool is_first = true;

			for(auto &item: *list)
			{
				auto value = std::dynamic_pointer_cast<Ttype>(item);

				if(value == nullptr)
				{
					OV_ASSERT2(false);
					return "";
				}

				if(is_first)
				{
					result.AppendFormat("%s", value->ToString(indent).CStr());
					is_first = false;
				}
				else
				{
					result.AppendFormat(", %s", value->ToString(indent).CStr());
				}
			}

			result.Append(append_new_line ? "]\n" : "]");

			return result;
		}

	protected:
		static constexpr ValueType Tconfig_type = ValueType::List;
		using ValueContainer<Tvector>::_value;
		using ValueContainer<Tvector>::_is_parsed;
	};
}