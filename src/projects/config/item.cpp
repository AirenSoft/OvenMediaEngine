//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "item.h"

#include "config_private.h"

namespace cfg
{
	static ov::String MakeIndentString(int indent)
	{
		static std::map<int, ov::String> cache;

		auto item = cache.find(indent);

		if(item != cache.end())
		{
			return item->second;
		}

		ov::String indent_string;

		for(int count = 0; count < indent; count++)
		{
			indent_string += "    ";
		}

		cache[indent] = indent_string;

		return indent_string;
	}

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

			case ValueType::Text:
				return "Text";

			case ValueType::Attribute:
				return "Attribute";

			case ValueType::Element:
				return "Element";

			case ValueType::List:
				return "List";
		}
	}

	Item::Item(const Item &item)
	{
		_tag_name = item._tag_name;

		for(auto &parse_item : item._parse_list)
		{
			_parse_list.emplace_back(parse_item.name, parse_item.is_parsed, nullptr);
		}

		_parent = item._parent;
		_parsed = item._parsed;
	}

	Item::Item(Item &&item) noexcept
	{
		std::swap(_tag_name, item._tag_name);
		std::swap(_parse_list, item._parse_list);
		std::swap(_parent, item._parent);
	}

	const ov::String Item::GetTagName() const
	{
		return _tag_name;
	}

	Item &Item::operator =(const Item &item)
	{
		_tag_name = item._tag_name;
		_parent = item._parent;
		_parse_list = item._parse_list;

		// To update pointers of ValueBase::_target
		MakeParseList();

		return *this;
	}


	void Item::Register(const ov::String &name, ValueBase *value, bool is_optional, bool is_includable, bool is_overridable) const
	{
		value->SetOptional(is_optional);
		value->SetIncludable(is_includable);
		value->SetOverridable(is_overridable);

		bool is_parsed = false;

		for(auto old_value = _parse_list.begin(); old_value != _parse_list.end(); ++old_value)
		{
			if(old_value->name == name)
			{
				// restore some values before overwrite
				is_parsed = old_value->is_parsed;
				ValueBase *base = old_value->value.get();

				if(base != nullptr)
				{
					// The attributes of two values must be the same
					OV_ASSERT2(value->GetType() == base->GetType());
					OV_ASSERT2(value->IsOptional() == base->IsOptional());
					OV_ASSERT2(value->IsIncludable() == base->IsIncludable());
					OV_ASSERT2(value->IsOverridable() == base->IsOverridable());
				}

				_parse_list.erase(old_value);
				break;
			}
		}

		_parse_list.emplace_back(name, is_parsed, value);
	}

	ValueBase *Item::FindValue(const ov::String &name, const ParseItem *parse_item_to_find)
	{
		for(auto &parse_item : _parse_list)
		{
			if(
				(parse_item.name == name) &&
				((parse_item.value != nullptr) && (parse_item.value->GetType() == parse_item_to_find->value->GetType()))
				)
			{
				return parse_item.value.get();
			}
		}

		if(_parent != nullptr)
		{
			return _parent->FindValue(name, parse_item_to_find);
		}

		return nullptr;
	}

	bool Item::IsParsed(const void *target) const
	{
		MakeParseList();

		for(auto &parse_item : _parse_list)
		{
			OV_ASSERT2(parse_item.value != nullptr);

			if(parse_item.value->GetTarget() == target)
			{
				return parse_item.is_parsed;
			}
		}

		return false;
	}

	bool Item::Parse(const ov::String &file_name, const ov::String &tag_name)
	{
		return ParseFromFile(file_name, tag_name, 0);
	}

	bool Item::IsParsed() const
	{
		return _parsed;
	}

	bool Item::ParseFromFile(const ov::String &file_name, const ov::String &tag_name, int indent)
	{
		_tag_name = tag_name;

		pugi::xml_document document;
		pugi::xml_parse_result result = document.load_file(file_name);

		if(result == false)
		{
			logte("Could not read the file (reason: %s)", result.description());
			return false;
		}

		return ParseFromNode(document.child(_tag_name), tag_name, false, indent);
	}

	// node는 this 레벨에 준하는 항목임. 즉, node.name() == _tag_name.CStr() 관계가 성립
	bool Item::ParseFromNode(const pugi::xml_node &node, const ov::String &tag_name, int indent)
	{
		return ParseFromNode(node, tag_name, false, indent);
	}

#define CONFIG_DECLARE_PROCESSOR(value_type, type, dst) \
        case value_type: \
        { \
            auto target = dynamic_cast<Value<type> *>(parse_item.value.get()); \
            \
            if((target != nullptr) && (target->GetTarget() != nullptr)) \
            { \
                *(target->GetTarget()) = dst; \
                parsed = true; \
                logtd("%s[%s] [%s] = %s", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(), ToString(&parse_item, 0, false).CStr()); \
            } \
            else \
            { \
                OV_ASSERT2(false); \
            } \
            \
            break; \
        }

#define CONFIG_FROM_ANOTHER_VALUE(value_type, type, src) \
        case value_type: \
        { \
            auto source = dynamic_cast<type *>(src); \
            auto target = dynamic_cast<type *>(parse_item.value.get()); \
            \
            if((source != nullptr) && (target != nullptr)) \
            { \
                OV_ASSERT2(source->GetTarget() != nullptr); \
                OV_ASSERT2(target->GetTarget() != nullptr); \
                *(target->GetTarget()) = *(source->GetTarget()); \
                parse_item.is_parsed = true; \
                logtd("%s[%s] [%s] = %s (from parent value)", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(), ToString(&parse_item, 0, false).CStr()); \
            } \
            else \
            { \
                OV_ASSERT(false, "Source: %p, Target: %p", source, target); \
            } \
            \
            break; \
        }

	bool Item::ParseFromNode(const pugi::xml_node &node, const ov::String &tag_name, bool process_include, int indent)
	{
		_tag_name = tag_name;
		_parsed = false;

		MakeParseList();

		logtd("%s[%s] Trying to parse node (this = %p)...", MakeIndentString(indent).CStr(), _tag_name.CStr(), this);

		auto inc = node.attribute("include");

		if(inc.empty() == false)
		{
			logtd("%s[%s] Include file found: %s", MakeIndentString(indent).CStr(), _tag_name.CStr(), inc.value());

			if(ParseFromFile(inc.value(), tag_name, indent) == false)
			{
				return false;
			}
		}

		for(auto &parse_item : _parse_list)
		{
			auto &name = parse_item.name;
			ValueBase *value = parse_item.value.get();

			if(value == nullptr)
			{
				OV_ASSERT(value != nullptr, "[%s] Cannot obtain value information for %s (this is a bug)", _tag_name.CStr(), name.CStr());
				logte("%s[%s] Cannot obtain value information for %s (this is a bug)", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr());
				continue;
			}

			pugi::xml_node child_node;
			pugi::xml_attribute attribute;

			switch(value->GetType())
			{
				case ValueType::Text:
					child_node = node;
					break;

				case ValueType::Attribute:
					attribute = node.attribute(name);
					break;

				default:
					child_node = node.child(name);
					break;
			}

			if(child_node.empty() && attribute.empty())
			{
				logtd("%s[%s] [%s] is empty", MakeIndentString(indent + 1).CStr(), _tag_name.CStr(), name.CStr());

				if(parse_item.is_parsed)
				{
					// the item was parsed from include file
					continue;
				}

				ValueBase *parent_value = nullptr;

				if(_parent != nullptr)
				{
					logtd("%s[%s] Trying to find [%s] from parent...", MakeIndentString(indent + 1).CStr(), _tag_name.CStr(), name.CStr());
					parent_value = _parent->FindValue(name, &parse_item);
				}

				if(parent_value == nullptr)
				{
					continue;
				}

				if(parse_item.value->GetSize() != parent_value->GetSize())
				{
					logte("Element size does not matched: %zu != %zu", parse_item.value->GetSize(), parent_value->GetSize());
					OV_ASSERT2(false);
					continue;
				}

				switch(parse_item.value->GetType())
				{
					CONFIG_FROM_ANOTHER_VALUE(ValueType::Integer, Value<int>, parent_value)
					CONFIG_FROM_ANOTHER_VALUE(ValueType::Boolean, Value<bool>, parent_value)
					CONFIG_FROM_ANOTHER_VALUE(ValueType::Float, Value<float>, parent_value)
					CONFIG_FROM_ANOTHER_VALUE(ValueType::String, Value<ov::String>, parent_value)
					CONFIG_FROM_ANOTHER_VALUE(ValueType::Text, Value<ov::String>, parent_value)
					CONFIG_FROM_ANOTHER_VALUE(ValueType::Attribute, Value<ov::String>, parent_value)

					case ValueType::Element:
					{
						auto source = dynamic_cast<ValueForElementBase *>(parent_value);
						auto target = dynamic_cast<ValueForElementBase *>(value);

						if(source != nullptr)
						{
							parse_item.is_parsed = source->Copy(target);

							if(parse_item.is_parsed)
							{
								logtd("%s[%s] [%s] = %s (from parent value)", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(), ToString(&parse_item, 0, false).CStr());
							}
						}
						else
						{
							OV_ASSERT(false, "Source: %p, Target: %p", source, target);
						}

						break;
					}
					case ValueType::List:
					{
						auto source = dynamic_cast<ValueForListBase *>(parent_value);
						auto target = dynamic_cast<ValueForListBase *>(value);

						if(source != nullptr)
						{
							parse_item.is_parsed = source->Copy(target);

							if(parse_item.is_parsed)
							{
								logtd("%s[%s] [%s] = %s (from parent value)", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(), ToString(&parse_item, 0, false).CStr());
							}
						}
						else
						{
							OV_ASSERT(false, "Source: %p, Target: %p", source, target);
						}

						break;
					}

					case ValueType::Unknown:
					default:
						OV_ASSERT2(false);
						break;
				}
			}
			else
			{
				bool parsed = false;

				logtd("%s[%s] Trying to parse value from node [%s]...", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr());

				switch(value->GetType())
				{
					CONFIG_DECLARE_PROCESSOR(ValueType::Integer, int, ov::Converter::ToInt32(child_node.child_value()))
					CONFIG_DECLARE_PROCESSOR(ValueType::Boolean, bool, ov::Converter::ToBool(child_node.child_value()))
					CONFIG_DECLARE_PROCESSOR(ValueType::Float, float, ov::Converter::ToFloat(child_node.child_value()))
					CONFIG_DECLARE_PROCESSOR(ValueType::String, ov::String, child_node.child_value())
					CONFIG_DECLARE_PROCESSOR(ValueType::Text, ov::String, child_node.child_value())
					CONFIG_DECLARE_PROCESSOR(ValueType::Attribute, ov::String, attribute.value())

					case ValueType::Element:
					{
						auto target = dynamic_cast<ValueForElementBase *>(value);

						if((target != nullptr) && (target->GetTarget() != nullptr))
						{
							target->GetTarget()->_parent = this;
							parsed = (target->GetTarget())->ParseFromNode(child_node, name, value->IsIncludable(), indent + 1);
						}
						else
						{
							OV_ASSERT2(false);
						}

						if(parsed)
						{
							logtd("%s[%s] [%s] = %s", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(), "{ ... }");
						}

						break;
					}

					case ValueType::List:
					{
						logtd("%s[%s] Trying to parse item list [%s]...", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr());

						auto target = dynamic_cast<ValueForListBase *>(value);

						if((target != nullptr) && (target->GetTarget() != nullptr))
						{
							if(value->IsOptional())
							{
								// optional이라면 먼저 parse 된 상태로 전환
								parsed = true;
							}

							while(child_node)
							{
								Item *i = target->Create();

								i->_parent = this;
								parsed = i->ParseFromNode(child_node, name, value->IsIncludable(), indent + 1);

								if(parsed == false)
								{
									break;
								}

								logtd("%s[%s] [List<%s>] = %s",
								      MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(),
								      ov::String::FormatString("{ %zu items }", target->GetList().size()).CStr()
								);

								child_node = child_node.next_sibling(name);
							}

						}
						else
						{
							OV_ASSERT2(false);
						}

						break;
					}

					case ValueType::Unknown:
						OV_ASSERT2(false);
						break;
				}

				if(parsed == false)
				{
					logte("%s[%s] Could not parse [%s]", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr());
				}

				parse_item.is_parsed = parsed;
			}
		}

		_parsed = true;
		return true;
	}

	ov::String Item::ToString() const
	{
		return ToString(0);
	}

	ov::String Item::ToString(int indent) const
	{
		ov::String result;

		MakeParseList();

		if(indent == 0)
		{
			result.AppendFormat("%s = ", _tag_name.CStr());
		}

		if(_parse_list.empty())
		{
			result.Append("{}");
		}
		else
		{
			result.Append("{\n");

			for(auto &parse_item : _parse_list)
			{
				result.Append(ToString(&parse_item, indent + 1, true));
			}

			result.AppendFormat("%s}", MakeIndentString(indent).CStr());
		}

		return result;
	}

	ov::String Item::ToString(const ParseItem *parse_item, int indent, bool append_new_line) const
	{
		ov::String str;

		ValueBase *value = parse_item->value.get();

		switch(value->GetType())
		{
			case ValueType::Integer:
			{
				auto target = dynamic_cast<Value<int> *>(value);
				str = (target != nullptr) ? ov::String::FormatString("%d", *(target->GetTarget())) : "(null)";
				break;
			}

			case ValueType::Boolean:
			{
				auto target = dynamic_cast<Value<bool> *>(value);
				str = (target != nullptr) ? (*(target->GetTarget()) ? "true" : "false") : "(null)";
				break;
			}

			case ValueType::Float:
			{
				auto target = dynamic_cast<Value<float> *>(value);
				str = (target != nullptr) ? ov::String::FormatString("%f", *(target->GetTarget())) : "(null)";
				break;
			}

			case ValueType::String:
			case ValueType::Attribute:
			case ValueType::Text:
			{
				auto target = dynamic_cast<Value<ov::String> *>(value);
				str = (target != nullptr) ? *(target->GetTarget()) : "(null)";
				break;
			}

			case ValueType::Element:
			{
				auto target = dynamic_cast<ValueForElementBase *>(value);
				str = (target != nullptr) ? target->GetTarget()->ToString(indent) : "(null)";
				break;
			}

			case ValueType::List:
			{
				auto target = dynamic_cast<ValueForListBase *>(value);

				ov::String indent_string = MakeIndentString(indent + 1);
				auto item_list = target->GetList();

				if(item_list.empty())
				{
					str = "(no items) []";
				}
				else
				{
					str = ov::String::FormatString("(%d items) [", item_list.size());
					bool is_first = true;

					for(auto &item: item_list)
					{
						if(item == nullptr)
						{
							OV_ASSERT2(false);
							return "";
						}

						if(is_first)
						{
							str.AppendFormat("%s", item->ToString(indent).CStr());
							is_first = false;
						}
						else
						{
							str.AppendFormat(", %s", item->ToString(indent).CStr());
						}
					}

					str.Append(']');
				}

				break;
			}

			default:
				break;

		}

		return ov::String::FormatString(
			"%s%s%s%s (%s%s%s) = %s%s",
			MakeIndentString(indent).CStr(),
			value->GetType() == ValueType::List ? "List<" : "",
			parse_item->name.CStr(),
			value->GetType() == ValueType::List ? ">" : "",
			GetValueTypeAsString(value->GetType()).CStr(),
			value->IsOptional() ? ", optional" : "",
			value->IsIncludable() ? ", includable" : "",
			parse_item->is_parsed ? str.CStr() : "N/A",
			append_new_line ? "\n" : ""
		);
	}
};