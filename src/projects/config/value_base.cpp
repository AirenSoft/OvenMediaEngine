//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "value_base.h"

#include "item.h"

namespace cfg
{
	static ov::String GetValueTypeAsString(ValueType type)
	{
		switch(type)
		{
			default:
			case ValueType::Unknown:
				return "Unknown";

			case ValueType::String:
				return "String";

			case ValueType::Integer:
				return "Integer";

			case ValueType::Boolean:
				return "Boolean";

			case ValueType::Float:
				return "Float";

			case ValueType::Element:
				return "Element";

			case ValueType::List:
				return "List";

			case ValueType::Attribute:
				return "Attribute";

			case ValueType::Text:
				return "Text";
		}
	}

	ValueType ValueBase::GetType() const
	{
		return _type;
	}

	void ValueBase::SetType(ValueType type)
	{
		_type = type;
	}

	ov::String ValueBase::GetName() const
	{
		return _name;
	}

	void ValueBase::SetName(const ov::String &name)
	{
		_name = name;
	}

	const Item *ValueBase::GetParent() const
	{
		return _parent;
	}

	Item *ValueBase::GetParent()
	{
		return _parent;
	}

	void ValueBase::SetParent(Item *parent)
	{
		_parent = parent;
	}

	bool ValueBase::operator ==(ValueType type) const
	{
		return _type == type;
	}

	bool ValueBase::operator !=(ValueType type) const
	{
		return !operator ==(type);
	}

	bool ValueBase::IsOptional() const
	{
		return _is_optional;
	}

	void ValueBase::SetOptional(bool is_optional)
	{
		_is_optional = is_optional;
	}

	bool ValueBase::IsIncludable() const
	{
		return _is_includable;
	}

	void ValueBase::SetIncludable(bool is_includable)
	{
		_is_includable = is_includable;
	}

	bool ValueBase::IsOverridable() const
	{
		return _is_overridable;
	}

	void ValueBase::SetOverridable(bool is_overridable)
	{
		_is_overridable = is_overridable;
	}

	bool ValueBase::IsParsed() const
	{
		return _is_parsed;
	}

	void ValueBase::SetParsed(bool is_parsed)
	{
		_is_parsed = is_parsed;
	}

	bool ValueBase::IsValid() const
	{
		return false;
	}

	ov::String ValueBase::ToString() const
	{
		return ToString(0, false);
	}

	ov::String ValueBase::ToString(int indent, bool append_new_line) const
	{
		return ov::String::FormatString(
			"%s%s%s%s (%s%s%s) = %s",
			MakeIndentString(indent).CStr(),
			_type == ValueType::List ? "List<" : "",
			_name.CStr(),
			_type == ValueType::List ? ">" : "",
			GetValueTypeAsString(_type).CStr(),
			_is_optional ? ", optional" : "",
			_is_includable ? ", includable" : "",
			_is_parsed ? ToStringInternal(indent, append_new_line).CStr() : "N/A\n"
		);
	}

	ov::String ValueBase::MakeIndentString(int indent)
	{
		ov::String indent_string;

		for(int count = 0; count < indent; count++)
		{
			indent_string += "    ";
		}

		return indent_string;
	}

	bool ValueBase::ProcessItem(Item *item, const pugi::xml_node &node, bool processing_include_file, int indent)
	{
		return
			item->PreProcess(_name, node, this, indent) &&
			item->ParseFromNode(node, _name, processing_include_file, indent) &&
			item->PostProcess(indent);
	}
}