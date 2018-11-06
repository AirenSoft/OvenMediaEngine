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
	void Item::SetParent(Item *parent)
	{
		_parent = parent;
	}

	Item *Item::GetParent()
	{
		return _parent;
	}

	const Item *Item::GetParent() const
	{
		return _parent;
	}

	bool Item::IsValid() const
	{
		bool is_valid = true;

		for(auto &value: _parse_list)
		{
			//is_valid &= value.second->IsValid(this);
		}

		return is_valid;
	}

	bool Item::PreProcess(const ov::String &tag_name, const pugi::xml_node &node, const ValueBase *value_type, int indent)
	{
		_tag_name = tag_name;

		if(value_type->IsIncludable())
		{
			logtd("%s[%s] Check includability...", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr());

			auto include_file = node.attribute("include");

			if(include_file.empty() == false)
			{
				logtd("%s[%s] Include file found: %s. Trying to parse from file...", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), include_file.value());

				bool result = ParseFromFile(include_file.value(), tag_name, true, indent + 1);

				if(result)
				{
					return IsValid();
				}
				else
				{
					// Cannot parse the XML file
					logte("An error occurred while parse the config %s from file: %s", include_file.value(), tag_name.CStr());
					return false;
				}
			}

			return true;
		}

		// not includable
		return true;
	}

	ValueBase *Item::FindParentValue(const ValueBase *value) const
	{
		auto item = _parse_list.find(value->GetName());
		bool find_from_parent = false;

		if(
			// 항목을 못찾거나
			(item == _parse_list.end()) ||
			// 찾았는데 타입이 안맞는 경우 부모에게 물어봐야 함
			(item->second->GetType() != value->GetType())
			)
		{
			// 부모 항목에서 값이 존재하는지 찾아봄
			if(_parent != nullptr)
			{
				return _parent->FindParentValue(value);
			}
			else
			{
				// This item has no parent
			}
		}
		else
		{
			// 항목을 찾았고 타입도 동일함

			// 이미 this를 파싱하면서, 값이 다 입력되었을 것이기 때문에, 굳이 parent에서 찾아보지 않아도 됨

			auto current_value = item->second;

			if(current_value->IsOverridable())
			{
				if(current_value->IsParsed())
				{
					// 값이 이미 파싱 된 상태라면, 그냥 넘김
					return current_value;
				}

				// optional이 아닌데, 값이 입력되어 있지 않다면 프로그램 버그임
				OV_ASSERT2(current_value->IsOptional());
			}
			else
			{
				// override 불가능한 값
			}
		}

		return nullptr;
	}

	// 파일로 부터 파싱 시작 (root element 이거나 include가 달려 있는 항목은 파일로 부터 파싱함)
	bool Item::Parse(const ov::String &file_name, const ov::String &tag_name)
	{
		return ParseFromFile(file_name, tag_name, false, 0);
	}

	bool Item::ParseFromFile(const ov::String &file_name, const ov::String &tag_name, bool processing_include_file, int indent)
	{
		_tag_name = tag_name;

		pugi::xml_document document;
		pugi::xml_parse_result result = document.load_file(file_name);

		if(result == false)
		{
			logte("Could not read the file (reason: %s)", result.description());
			return false;
		}

		return ParseFromNode(document.child(_tag_name), tag_name, processing_include_file, indent);
	}

	// node는 this 레벨에 준하는 항목임. 즉, node.name() == _tag_name.CStr() 관계가 성립
	bool Item::ParseFromNode(const pugi::xml_node &node, const ov::String &tag_name, int indent)
	{
		return ParseFromNode(node, tag_name, false, indent);
	}

	bool Item::ParseFromNode(const pugi::xml_node &node, const ov::String &tag_name, bool processing_include_file, int indent)
	{
		_tag_name = tag_name;

		if(MakeParseList() == false)
		{
			return false;
		}

		logtd("%s[%s] Trying to parse children (this = %p, from_include = %d)...", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), this, processing_include_file);

		for(auto &value : _parse_list)
		{
			// value.first에 해당하는 값을 찾음
			auto &child_name = value.first;
			auto &child_value = value.second;

			pugi::xml_node child_node;
			pugi::xml_attribute attribute;

			switch(child_value->GetType())
			{
				case ValueType::Attribute:
					attribute = node.attribute(child_name);
					break;

				case ValueType::Text:
					child_node = node;
					break;

				default:
					child_node = node.child(child_name);
					break;
			}

			if(child_node.empty() && attribute.empty())
			{
				logtd("%s[%s] [%s] is empty", ValueBase::MakeIndentString(indent + 1).CStr(), _tag_name.CStr(), child_name.CStr());

				// 만약, <ConfigItem>의 <Name>이 필수 값이라고 할 때, Config이 다음과 같이 설정되어 있다면,
				//
				// a.xml
				// ---------------
				// <ConfigItem>
				//   <Name>NameInA</Name>
				// </ConfigItem>
				//
				// config.xml
				// ---------------
				// <ConfigItem include="a.xml">
				// </ConfigItem>
				//
				// a.xml에서 Name 값이 지정되었으므로, config.xml을 파싱하는 단계에서 Name을 찾지 못하더라도 그건 오류가 아님
				// 이것을 판단하기 위해, 기존에 값이 설정된 적이 있는지를 보면 됨

				if(child_value->IsParsed())
				{
					// include된 파일로 부터 값이 이미 설정됨. 성공으로 간주함
					continue;
				}

				ValueBase *parent_value = nullptr;

				if(_parent != nullptr)
				{
					logtd("%s[%s] Trying to find [%s] from parent...", ValueBase::MakeIndentString(indent + 1).CStr(), _tag_name.CStr(), child_name.CStr());
					parent_value = _parent->FindParentValue(child_value);
				}

				if(parent_value == nullptr)
				{
					if((processing_include_file == false) && (child_value->IsOptional() == false))
					{
						// 필수 항목인데, 부모에도 값이 지정되어 있지 않음
						logte("Cannot find element: %s->%s", _tag_name.CStr(), child_name.CStr());
						return false;
					}

					// 필수 항목이 아니거나, include 파일로 부터 읽고 있는 상황이기 때문에 그냥 진행
					if(child_value->IsOptional())
					{
						logtd("%s[%s] Ignoring [%s] (optional)...", ValueBase::MakeIndentString(indent + 2).CStr(), _tag_name.CStr(), child_name.CStr());
					}

					continue;
				}

				// 부모로부터 값을 얻어왔음
				logtd("%s[%s] From parent: [%s] = %s", ValueBase::MakeIndentString(indent + 1).CStr(), _tag_name.CStr(), child_name.CStr(), parent_value->ToString().CStr());

				switch(value.second->GetType())
				{
					case ValueType::String:
					case ValueType::Integer:
					case ValueType::Boolean:
					case ValueType::Float:
					case ValueType::Element:
					case ValueType::List:
					case ValueType::Attribute:
					case ValueType::Text:
						if(child_value->ParseFromValue(parent_value, indent + 1) == false)
						{
							return false;
						}

						break;

					case ValueType::Unknown:
					default:
						OV_ASSERT2(false);
						break;
				}
			}
			else
			{
				switch(value.second->GetType())
				{
					case ValueType::String:
					case ValueType::Integer:
					case ValueType::Boolean:
					case ValueType::Float:
					case ValueType::Text:
						logtd("%s[%s] Trying to parse plain value from node [%s]...", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), child_node.name());
						if(child_value->ParseFromNode(child_node, processing_include_file, indent) == false)
						{
							return false;
						}

						logtd("%s[%s] [%s] = %s", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), child_node.name(), child_value->ToString().CStr());
						break;

					case ValueType::Element:
						logtd("%s[%s] Trying to parse item from node [%s]...", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), child_node.name());
						if(child_value->ParseFromNode(child_node, processing_include_file, indent) == false)
						{
							return false;
						}

						logtd("%s[%s] [%s] = %s", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), child_node.name(), child_value->IsParsed() ? "{}" : "N/A");
						break;

					case ValueType::List:
					{
						logtd("%s[%s] Trying to parse item list [%s]...", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), child_node.name());

						while(child_node)
						{
							if(child_value->ParseFromNode(child_node, processing_include_file, indent) == false)
							{
								return false;
							}

							logtd("%s[%s] [%s] = ", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), child_node.name(), child_value->IsParsed() ? "{ ... }" : "N/A");
							child_node = child_node.next_sibling(child_name);
						}
					}
						break;

					case ValueType::Attribute:
						logtd("%s[%s] Trying to parse item from attribute [%s]...", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), attribute.name());
						if(child_value->ParseFromAttribute(attribute, processing_include_file, indent) == false)
						{
							return false;
						}
						logtd("%s[%s] [%s] = %s", ValueBase::MakeIndentString(indent).CStr(), _tag_name.CStr(), child_node.name(), child_value->ToString().CStr());
						break;

					case ValueType::Unknown:
						OV_ASSERT2(false);
						break;
				}
			}
		}

		return true;
	}

	bool Item::PostProcess(int indent)
	{
		return true;
	}

	ov::String Item::ToString() const
	{
		return ToString(0);
	}

	ov::String Item::ToString(int indent) const
	{
		ov::String result;

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

			for(auto &value : _parse_list)
			{
				result.AppendFormat("%s", value.second->ToString(indent + 1, true).CStr());
			}

			result.AppendFormat("%s}", ValueBase::MakeIndentString(indent).CStr());
		}

		return result;
	}
};