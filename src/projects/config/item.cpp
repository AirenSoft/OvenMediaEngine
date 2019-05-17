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

#include <regex>

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
			case ValueType::Unknown:
				break;

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

		return "Unknown";
	}

	Item::Item(const Item &item)
	{
		_tag_name = item._tag_name;

		for(auto &parse_item : item._parse_list)
		{
			_parse_list.emplace_back(parse_item.name, parse_item.is_parsed, parse_item.from_default, nullptr);
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

	const Item *Item::GetParent(const ov::String &name) const
	{
		if(_parent != nullptr)
		{
			if(_parent->GetTagName() == name)
			{
				return _parent;
			}

			return _parent->GetParent(name);
		}

		return nullptr;
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


	void Item::Register(const ov::String &name, ValueBase *value, bool is_optional, bool need_to_resolve_path) const
	{
		value->SetOptional(is_optional);
		value->SetNeedToResolvePath(need_to_resolve_path);

		bool is_parsed = false;
		bool from_default = true;

		for(auto old_value = _parse_list.begin(); old_value != _parse_list.end(); ++old_value)
		{
			if(old_value->name == name)
			{
				// restore some values before overwrite
				is_parsed = old_value->is_parsed;
				from_default = old_value->from_default;

				ValueBase *base = old_value->value.get();

				if(base != nullptr)
				{
					// The attributes of two values must be the same
					OV_ASSERT2(value->GetType() == base->GetType());
					OV_ASSERT2(value->IsOptional() == base->IsOptional());
					OV_ASSERT2(value->IsNeedToResolvePath() == base->IsNeedToResolvePath());
				}

				_parse_list.erase(old_value);
				break;
			}
		}

		_parse_list.emplace_back(name, is_parsed, from_default, value);
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
		return ParseFromFile("", file_name, tag_name, 0) != ParseResult::Error;
	}

	bool Item::IsParsed() const
	{
		return _parsed;
	}

	bool Item::IsDefault(const void *target) const
	{
		MakeParseList();

		for(auto &parse_item : _parse_list)
		{
			OV_ASSERT2(parse_item.value != nullptr);

			if(parse_item.value->GetTarget() == target)
			{
				return parse_item.from_default;
			}
		}

		return false;
	}

	Item::ParseResult Item::ParseFromFile(const ov::String &base_file_name, ov::String file_name, const ov::String &tag_name, int indent)
	{
		_tag_name = tag_name;

		if((ov::PathManager::IsAbsolute(file_name) == false) && (base_file_name.IsEmpty() == false))
		{
			file_name = ov::PathManager::Combine(ov::PathManager::ExtractPath(base_file_name), file_name);
		}

		pugi::xml_document document;
		pugi::xml_parse_result result = document.load_file(file_name);

		if(result == false)
		{
			logte("Could not read the file: %s (reason: %s)", file_name.CStr(), result.description());
			return ParseResult::Error;
		}

		return ParseFromNode(file_name, document.child(_tag_name), tag_name, indent);
	}

#define CONFIG_DECLARE_PROCESSOR(value_type, type, converter, process_value) \
        case value_type: \
        { \
            auto target = dynamic_cast<Value<type> *>(parse_item.value.get()); \
            \
            if((target != nullptr) && (target->GetTarget() != nullptr)) \
            { \
                auto temp_value = process_value; \
                if((temp_value != nullptr) && (temp_value[0] != '\0')) \
                { \
                    *(target->GetTarget()) = converter(Preprocess(base_file_name, value, temp_value)); \
                    logtd("%s<%s> <%s> = %s", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(), ToString(&parse_item, 0, true, false).CStr()); \
                    parse_item.from_default = false; \
                } \
                else \
                { \
                    logtd("%s<%s> <%s> = %s (use default value)", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(), ToString(&parse_item, 0, true, false).CStr()); \
                } \
                parse_item.is_parsed = true; \
                result = ParseResult::Parsed; \
            } \
            else \
            { \
                OV_ASSERT2(false); \
                return ParseResult::Error; \
            } \
            \
            break; \
        }

	// node는 this 레벨에 준하는 항목임. 즉, node.name() == _tag_name.CStr() 관계가 성립
	Item::ParseResult Item::ParseFromNode(const ov::String &base_file_name, const pugi::xml_node &node, const ov::String &tag_name, int indent)
	{
		_tag_name = tag_name;
		_parsed = false;

		MakeParseList();

		logtd("%s<%s> Trying to parse node (this = %p)...", MakeIndentString(indent).CStr(), _tag_name.CStr(), this);

		auto inc = node.attribute("include");

		if(inc.empty() == false)
		{
			logtd("%s<%s> Include file found: %s", MakeIndentString(indent).CStr(), _tag_name.CStr(), inc.value());

			auto result = ParseFromFile(base_file_name, inc.value(), tag_name, indent + 1);

			if(result == ParseResult::Error)
			{
				return ParseResult::Error;
			}

			_parsed = (result == ParseResult::Parsed);
		}

		// Check for invalid XML elements
		{
			for(auto &child : node.children())
			{
				auto name = child.name();

				auto item = std::find_if(_parse_list.begin(), _parse_list.end(), [&](const ParseItem &item) -> bool
				{
					return item.name == name;
				});

				if(item == _parse_list.end())
				{
					logte("Unknown configuration found: %s", GetSelector(child).CStr());
					return ParseResult::Error;
				}
			}
		}

		// Parse the items from node
		for(auto &parse_item : _parse_list)
		{
			auto &name = parse_item.name;
			ValueBase *value = parse_item.value.get();
			auto result = ParseResult::NotParsed;

			if(value == nullptr)
			{
				logte("%s<%s> Cannot obtain value for <%s> (this is a bug)", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr());
				return ParseResult::Error;
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

			if(
				((value->GetType() != ValueType::Attribute) && (child_node.empty() == false)) ||
				((value->GetType() == ValueType::Attribute) && (attribute.empty() == false))
				)
			{
				switch(value->GetType())
				{
					CONFIG_DECLARE_PROCESSOR(ValueType::Integer, int, ov::Converter::ToInt32, child_node.child_value())
					CONFIG_DECLARE_PROCESSOR(ValueType::Boolean, bool, ov::Converter::ToBool, child_node.child_value())
					CONFIG_DECLARE_PROCESSOR(ValueType::Float, float, ov::Converter::ToFloat, child_node.child_value())
					CONFIG_DECLARE_PROCESSOR(ValueType::String, ov::String, , child_node.child_value())
					CONFIG_DECLARE_PROCESSOR(ValueType::Text, ov::String, , child_node.child_value())
					CONFIG_DECLARE_PROCESSOR(ValueType::Attribute, ov::String, , attribute.value())

					case ValueType::Element:
					{
						auto target = dynamic_cast<ValueForElementBase *>(value);

						if((target != nullptr) && (target->GetTarget() != nullptr))
						{
							target->GetTarget()->_parent = this;

							result = (target->GetTarget())->ParseFromNode(base_file_name, child_node, name, indent + 1);

							if(result == ParseResult::Error)
							{
								return result;
							}

							parse_item.is_parsed = (result == ParseResult::Parsed);
						}
						else
						{
							OV_ASSERT2(false);
							return ParseResult::Error;
						}

						logtd("%s<%s> <%s> = %s", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(), "{ ... }");

						break;
					}

					case ValueType::List:
					{
						logtd("%s<%s> Trying to parse item list <%s>...", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr());

						auto target = dynamic_cast<ValueForListBase *>(value);

						if((target != nullptr) && (target->GetTarget() != nullptr))
						{
							if((child_node.empty() == false) && (_parsed == false))
							{
								// Clear only when _parsed is false, as there may be a list of included items.
								target->Clear();
							}

							while(child_node)
							{
								Item *i = target->Create();

								i->_parent = this;

								result = i->ParseFromNode(base_file_name, child_node, name, indent + 1);

								if(result == ParseResult::Error)
								{
									return result;
								}

								parse_item.is_parsed = (result == ParseResult::Parsed);

								logtd("%s<%s> [List<%s>] = %s",
								      MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr(),
								      ov::String::FormatString("{ %zu items }", target->GetList().size()).CStr()
								);

								child_node = child_node.next_sibling(name);
							}
						}
						else
						{
							OV_ASSERT2(false);
							return ParseResult::Error;
						}

						break;
					}

					case ValueType::Unknown:
						OV_ASSERT2(false);
						return ParseResult::Error;
				}
			}

			if(result == ParseResult::Parsed)
			{
				parse_item.from_default = false;
			}

			// Check parsed from include file
			if(_parsed == false)
			{
				// It may parsed from XML, not include file
				if((value->IsOptional() == false) && (result != ParseResult::Parsed))
				{
					logtd("%s<%s> Could not parse node <%s>", MakeIndentString(indent).CStr(), _tag_name.CStr(), name.CStr());

					logte("%s.<%s> is required. Please check your configuration.", GetSelector(node).CStr(), name.CStr());
					return ParseResult::Error;
				}
			}
		}

		_parsed = true;
		return ParseResult::Parsed;
	}

	ov::String Item::Preprocess(const ov::String &xml_path, const ValueBase *value_base, const char *value)
	{
		// Preprocess for ${env:XXX} macro
		std::string str = value;
		std::string result;
		std::regex r(R"(\$\{env:([^}]*)\})");

		auto start = str.cbegin();
		std::sregex_iterator iterator = std::sregex_iterator(start, str.cend(), r);

		while(iterator != std::sregex_iterator())
		{
			std::smatch matches = *iterator;
			auto match = matches[0];
			auto token = matches[1];

			auto env_key = token.str();
			auto position = env_key.find_first_of(':');

			std::string default_value;
			bool is_default_value = false;

			if(position != std::string::npos)
			{
				default_value = env_key.substr(position + 1);
			}

			const char *key = env_key.c_str();
			const char *env = getenv(key);
			if(env == nullptr)
			{
				if(default_value.empty())
				{
					env = "";
				}
				else
				{
					is_default_value = true;
					env = default_value.c_str();
				}
			}

			logti("Configuration: %s = \"%s\"%s", key, env, is_default_value ? " (default value)" : "");

			result.append(start, match.first);
			result.append(env);

			start = match.second;

			++iterator;
		}

		result.append(start, str.cend());

		// Preprocess for ResolvePath annotation
		if(value_base->IsNeedToResolvePath())
		{
			const char *path = result.c_str();

			if(ov::PathManager::IsAbsolute(path) == false)
			{
				// relative path
				ov::String base_path = ov::PathManager::ExtractPath(xml_path);

				return ov::PathManager::Combine(base_path, path);
			}
		}

		return result.c_str();
	}

	ov::String Item::GetSelector(const pugi::xml_node &node)
	{
		std::vector<ov::String> selectors;

		selectors.emplace_back(node.name());

		auto parent = node.parent();

		while(
			(parent.type() != pugi::xml_node_type::node_null) &&
			(parent.type() != pugi::xml_node_type::node_document)
			)
		{
			selectors.insert(selectors.begin(), parent.name());

			parent = parent.parent();
		}

		ov::String selector = "<";
		selector.Append(ov::String::Join(selectors, ">.<"));
		selector.Append('>');

		return selector;
	}

	ov::String Item::ToString(bool exclude_default) const
	{
		return ToString(0, exclude_default);
	}

	ov::String Item::ToString(int indent, bool exclude_default) const
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
				result.Append(ToString(&parse_item, indent + 1, exclude_default, true));
			}

			result.AppendFormat("%s}", MakeIndentString(indent).CStr());
		}

		return result;
	}

	ov::String Item::ToString(const ParseItem *parse_item, int indent, bool exclude_default, bool append_new_line) const
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
				str = (target != nullptr) ? target->GetTarget()->ToString(indent, exclude_default) : "(null)";
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
							str.AppendFormat("%s", item->ToString(indent, exclude_default).CStr());
							is_first = false;
						}
						else
						{
							str.AppendFormat(", %s", item->ToString(indent, exclude_default).CStr());
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
			"%s%s%s%s (%s%s%s%s%s) = %s%s",
			MakeIndentString(indent).CStr(),
			value->GetType() == ValueType::List ? "List<" : "",
			parse_item->name.CStr(),
			value->GetType() == ValueType::List ? ">" : "",
			GetValueTypeAsString(value->GetType()).CStr(),
			value->IsOptional() ? ", optional" : "",
			value->IsNeedToResolvePath() ? ", path" : "",
			parse_item->is_parsed ? ", parsed" : "",
			parse_item->from_default ? ", default" : "",
			exclude_default ? (parse_item->is_parsed ? str.CStr() : "N/A") : str.CStr(),
			append_new_line ? "\n" : ""
		);
	}
};