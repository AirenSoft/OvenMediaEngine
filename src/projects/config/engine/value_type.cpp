//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "value_type.h"

#include "item.h"

namespace cfg
{
	const char *StringFromValueType(ValueType type)
	{
		switch (type)
		{
			case ValueType::Unknown:
				break;

			case ValueType::String:
				return "String";

			case ValueType::Integer:
				return "Integer";

			case ValueType::Long:
				return "Long";

			case ValueType::Boolean:
				return "Boolean";

			case ValueType::Double:
				return "Double";

			case ValueType::Attribute:
				return "Attribute";

			case ValueType::Text:
				return "Text";

			case ValueType::Item:
				return "Item";

			case ValueType::List:
				return "List";
		}

		return "Unknown";
	}

	constexpr const char *INDENTATION = "    ";
	ov::String MakeIndentString(int indent_count)
	{
		if (indent_count > 0)
		{
			ov::String indent_string;

			for (int index = 0; index < indent_count; index++)
			{
				indent_string += INDENTATION;
			}

			return indent_string;
		}

		return "";
	}

	ov::String ToString(const int *value)
	{
		return ov::Converter::ToString(*value);
	}

	ov::String ToString(const int64_t *value)
	{
		auto result = ov::Converter::ToString(*value);
		result += "L";
		return result;
	}

	ov::String ToString(const float *value)
	{
		auto result = ov::Converter::ToString(*value);
		result += "f";
		return result;
	}

	ov::String ToString(const double *value)
	{
		return ov::Converter::ToString(*value);
	}

	ov::String ToString(const ov::String *value)
	{
		ov::String result;
		result.Format("\"%s\"", value->CStr());
		return result;
	}

	ov::String ToString(int indent_count, const Item *object)
	{
		return object->ToString(indent_count);
	}
}  // namespace cfg
