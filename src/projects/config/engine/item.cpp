//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "item.h"

#include "../config_private.h"

namespace cfg
{
	ov::String StringFromJsonValue(const Json::Value &value)
	{
		if (value.isNull())
		{
			return "";
		}

		if (value.isString())
		{
			return value.asCString();
		}

		return "";
	}

	bool GetIncludeFileList(const DataSource &data_source, ov::String *pattern, std::vector<ov::String> *include_file_list)
	{
		Json::Value dummy_value;
		auto include_file = data_source.GetValue(ValueType::Attribute, "include", false, OmitRule::DontOmit, &dummy_value);

		if (include_file.has_value())
		{
			auto current_path = data_source.GetCurrentPath();

			// Load from the include file
			ov::String include_file_path = TryCast<ov::String>(include_file);

			logtd("Include file found: %s", include_file_path.CStr());

			std::vector<ov::String> file_list;
			auto path_error = ov::PathManager::GetFileList(current_path, include_file_path, &file_list);

			if (path_error != nullptr)
			{
				throw CreateConfigError("Could not obtain file list: current path: %s, include pattern: %s (%s)", current_path.CStr(), include_file_path.CStr(), path_error->ToString().CStr());
			}

			if (pattern != nullptr)
			{
				*pattern = include_file_path;
			}

			if (include_file_list != nullptr)
			{
				*include_file_list = std::move(file_list);
			}
			return true;
		}

		return false;
	}

	ov::String Item::ChildToString(int indent_count, const std::shared_ptr<Child> &child, size_t index, size_t child_count)
	{
		ov::String indent = MakeIndentString(indent_count + 1);

		ov::String description;
		ov::String extra;

		ov::String child_name = child->GetName().ToString();
		const std::any &child_target = child->GetTarget();
		auto value_type = child->GetType();

		ov::String comma = (index < (child_count - 1)) ? ", " : "";

#if CFG_VERBOSE_STRING
		extra.Format(
			CFG_EXTRA_PREFIX
			"%p: "
			"%s%s%s (%s), "
			"%s"
			"%s"
			"%s",
			child,
			(value_type == ValueType::List) ? "std::vector<" : "", child->GetTypeName().CStr(), (value_type == ValueType::List) ? ">" : "", StringFromValueType(child->GetType()),
			child->IsOptional() ? "Optional, " : "",
			child->ResolvePath() ? "Path, " : "",
			child->IsParsed() ? "DataSource" : "Default");

		if (child->IsParsed() &&
			((child->GetType() != ValueType::Item) && (child->GetType() != ValueType::List)))
		{
			extra.AppendFormat(" - \"%s\"", StringFromJsonValue(child->GetOriginalValue()).CStr());
		}

		extra.Append(CFG_EXTRA_SUFFIX);
#endif	// CFG_VERBOSE_STRING

		switch (value_type)
		{
			case ValueType::Unknown:
				[[fallthrough]];
			default:
				description.AppendFormat(
					"%s"
					"%s: <Unknown type>"
					"%s%s",
					indent.CStr(),
					child_name.CStr(),
					comma.CStr(), extra.CStr());
				break;

			case ValueType::String:
				description.AppendFormat(
					"%s"
					"%s: \"%s\""
					"%s%s",
					indent.CStr(),
					child_name.CStr(), TryCast<ov::String *>(child_target)->CStr(),
					comma.CStr(), extra.CStr());
				break;

			case ValueType::Integer:
				description.AppendFormat(
					"%s"
					"%s: %d"
					"%s%s",
					indent.CStr(),
					child_name.CStr(), *(TryCast<int *>(child_target)),
					comma.CStr(), extra.CStr());
				break;

			case ValueType::Long:
				description.AppendFormat(
					"%s"
					"%s: %ldL"
					"%s%s",
					indent.CStr(),
					child_name.CStr(), *(TryCast<int64_t *>(child_target)),
					comma.CStr(), extra.CStr());
				break;

			case ValueType::Boolean:
				description.AppendFormat(
					"%s"
					"%s: %s"
					"%s%s",
					indent.CStr(),
					child_name.CStr(), *(TryCast<bool *>(child_target)) ? "true" : "false",
					comma.CStr(), extra.CStr());
				break;

			case ValueType::Double:
				description.AppendFormat(
					"%s"
					"%s: %f"
					"%s%s",
					indent.CStr(),
					child_name.CStr(), *(TryCast<double *>(child_target)),
					comma.CStr(), extra.CStr());
				break;

			case ValueType::Attribute:
				description.AppendFormat(
					"%s"
					"$.%s: \"%s\""
					"%s%s",
					indent.CStr(),
					child_name.CStr(), TryCast<Attribute *>(child_target)->GetValue().CStr(),
					comma.CStr(), extra.CStr());
				break;

			case ValueType::Text:
				description.AppendFormat(
					"%s"
					"[%s]: \"%s\""
					"%s%s",
					indent.CStr(),
					child_name.CStr(), TryCast<Text *>(child_target)->ToString().CStr(),
					comma.CStr(), extra.CStr());
				break;

			case ValueType::Item:
				description.AppendFormat(
					"%s"
					"%s",
					TryCast<Item *>(child_target)->ToString(indent_count + 1).CStr(),
					comma.CStr());
				break;

			case ValueType::List: {
				auto list_item = TryCast<std::shared_ptr<ListInterface>>(child_target);

				if (list_item->GetCount() > 0)
				{
					description.AppendFormat(
						"%s"
						"%s: [%s\n%s"
						"%s]"
						"%s",
						indent.CStr(),
						child_name.CStr(), extra.CStr(),
						list_item->ToString(indent_count + 2, child).CStr(),
						indent.CStr(),
						comma.CStr());
				}
				else
				{
					description.AppendFormat(
						"%s"
						"%s: []"
						"%s%s",
						indent.CStr(),
						child_name.CStr(),
						comma.CStr(), extra.CStr());
				}

				break;
			}
		}

		return description;
	}

	template <class T>
	bool ValidateOmitRule(OmitRule omit_rule, const std::vector<T> &children)
	{
		if ((omit_rule == OmitRule::DontOmit) || (children.size() == 1))
		{
			// The item may be omitted
			return true;
		}

		return false;
	}

	Item::Item(const Item &item)
	{
		_need_to_update_list = true;

		_is_parsed = item._is_parsed;

		_item_name = item._item_name;

		_children = item._children;
		_children_for_xml = item._children_for_xml;
		_children_for_json = item._children_for_json;
	}

	Item::Item(Item &&item)
	{
		std::swap(_need_to_update_list, item._need_to_update_list);

		std::swap(_is_parsed, item._is_parsed);

		std::swap(_item_name, item._item_name);

		std::swap(_children, item._children);
		std::swap(_children_for_xml, item._children_for_xml);
		std::swap(_children_for_json, item._children_for_json);
	}

	Item &Item::operator=(const Item &item)
	{
		_need_to_update_list = true;

		_is_parsed = item._is_parsed;

		_item_name = item._item_name;

		_children = item._children;
		_children_for_xml = item._children_for_xml;
		_children_for_json = item._children_for_json;

		RebuildListIfNeeded();

		return *this;
	}

	void Item::RebuildListIfNeeded() const
	{
		// Change constness
		(const_cast<Item *>(this))->RebuildListIfNeeded();
	}

	void Item::RebuildListIfNeeded()
	{
		if (_need_to_update_list)
		{
			logtd("Rebuilding a list of children for: %s", _item_name.ToString().CStr());

			MakeList();

			_need_to_update_list = false;
		}
	}

	void Item::AddChild(const ItemName &name, ValueType type, const ov::String &type_name,
						bool is_optional, bool resolve_path,
						OptionalCallback optional_callback, ValidationCallback validation_callback,
						const void *raw_target, std::any target)
	{
		auto old_child_iterator = _children_for_xml.find(name.xml_name);

		if (old_child_iterator == _children_for_xml.end())
		{
			auto child = std::make_shared<Child>(
				name, type, type_name,
				is_optional, resolve_path,
				optional_callback, validation_callback,
				raw_target, std::move(target));

			_children_for_xml[name.xml_name] = child;
			_children_for_json[name.json_name] = child;
			_children.push_back(child);
		}
		else
		{
			// Reuse previous value to keep some variables such as _is_parsed
			auto &old_child = old_child_iterator->second;

			OV_ASSERT2(old_child->GetName() == name);
			OV_ASSERT2(old_child->GetType() == type);
			OV_ASSERT2(old_child->GetTypeName() == type_name);
			OV_ASSERT2(old_child->IsOptional() == is_optional);
			OV_ASSERT2(old_child->ResolvePath() == resolve_path);

			if (type == ValueType::List)
			{
				// Copy the children
				TryCast<std::shared_ptr<ListInterface>>(target)->CopyChildrenFrom(old_child->GetTarget());
			}

			old_child->Update(optional_callback, validation_callback, raw_target, target);
		}
	}

	void Item::FromDataSource(ov::String item_path, const ItemName &name, const DataSource &data_source)
	{
		logtd("[%s] Validating data source for: %s", item_path.CStr(), name.ToString().CStr());
		ValidateOmitRules(item_path);

		RebuildListIfNeeded();
		data_source.CheckUnknownItems(data_source.GetFileName(), item_path, _children_for_xml, _children_for_json);

		_item_name = name;

		FromDataSourceInternal(item_path, data_source);
	}

	bool Item::IsParsed(const void *target) const
	{
		RebuildListIfNeeded();

		for (auto &child : _children)
		{
			if (child->GetRawTarget() == target)
			{
				return child->IsParsed();
			}
		}

		throw CreateConfigError("Could not find target: %p", target);
	}

	void Item::ValidateOmitRule(const ov::String &item_path, const ov::String &name) const
	{
		if (_item_name.omit_rule == OmitRule::Omit)
		{
			if (_children.size() > 1)
			{
				OV_ASSERT(false, "\"%s\" item cannot be omitted because it has multiple children", item_path.CStr());

				throw CreateConfigError("[%s] This item cannot be omitted because it has multiple children", item_path.CStr());
			}
		}
		else
		{
			// Don't need to check omit rule for this item
		}

		for (const auto &child : _children)
		{
			switch (child->GetType())
			{
				case ValueType::Unknown:
				case ValueType::String:
				case ValueType::Integer:
				case ValueType::Long:
				case ValueType::Boolean:
				case ValueType::Double:
				case ValueType::Attribute:
				case ValueType::Text:
					break;

				case ValueType::Item: {
					try
					{
						auto child_item = TryCast<Item *>(child->GetTarget());
						child_item->ValidateOmitRule(item_path, child->GetName().ToString());
					}
					catch (const CastException &cast_exception)
					{
						throw CreateConfigError("[%s.%s] Could not cast %s to %s", item_path, child->GetName().ToString(), cast_exception.from.CStr(), cast_exception.to.CStr());
					}

					break;
				}

				case ValueType::List: {
					auto child_name = child->GetName().ToString();

					std::shared_ptr<ListInterface> list_interface;

					try
					{
						list_interface = TryCast<std::shared_ptr<ListInterface>>(child->GetTarget());
					}
					catch (const CastException &cast_exception)
					{
						throw CreateConfigError("[%s.%s] Could not cast %s to %s", item_path.CStr(), child_name.CStr(), cast_exception.from.CStr(), cast_exception.to.CStr());
					}

					auto list_value_type = list_interface->GetValueType();

					switch (list_value_type)
					{
						case ValueType::Unknown:
						case ValueType::String:
						case ValueType::Integer:
						case ValueType::Long:
						case ValueType::Boolean:
						case ValueType::Double:
						case ValueType::Attribute:
						case ValueType::Text:
							break;

						case ValueType::Item: {
							list_interface->ValidateOmitRule(item_path);
							break;
						}

						case ValueType::List:
							// Config module doesn't support nested list
							OV_ASSERT2(false);
							break;
					}

					break;
				}
			}
		}
	}

	void Item::ValidateOmitRules(const ov::String &item_path) const
	{
		RebuildListIfNeeded();

		ValidateOmitRule(item_path, _item_name.GetName(cfg::DataType::Json));
	}

	ov::String Item::ToString(int indent_count) const
	{
		RebuildListIfNeeded();

		ov::String indent = MakeIndentString(indent_count);

		auto item_name = _item_name.ToString();
		size_t child_count = _children.size();

		ov::String description;
		ov::String extra;

#if CFG_VERBOSE_STRING
		extra.Format(
			CFG_EXTRA_PREFIX
			"%p: "
			"%s, "
			"children = %zu, "
			"%s",
			this, ov::Demangle(typeid(*this).name()).CStr(),
			child_count,
			_is_parsed ? "DataSource" : "Default");
#endif	// CFG_VERBOSE_STRING

		description.AppendFormat(
			"%s"
			"%s: {%s"
			"%s" CFG_EXTRA_SUFFIX,
			indent.CStr(),
			item_name.CStr(), (child_count == 0) ? "}" : "",
			extra.CStr());

		size_t index = 0;

		for (auto &child : _children)
		{
			description.AppendFormat("\n%s",
									 ChildToString(indent_count, child, index, child_count).CStr());

			index++;
		}

		if (child_count > 0)
		{
			description.AppendFormat("\n%s}", indent.CStr());
		}

		return description;
	}

	void SetJsonChildValue(Json::Value &object, const ov::String &child_name, const Json::Value &value)
	{
		if (object.isArray())
		{
			object.append(value);
		}
		else
		{
			object[child_name] = value;
		}
	}

	void Item::AddJsonChild(Json::Value &object, ValueType value_type, OmitRule omit_rule, const ov::String &child_name, const std::any &child_target, const Json::Value &original_value, bool include_default_values)
	{
		switch (value_type)
		{
			case ValueType::Unknown:
				SetJsonChildValue(object, child_name, Json::nullValue);
				break;

			case ValueType::String:
				SetJsonChildValue(object, child_name, original_value);
				break;

			case ValueType::Integer:
				SetJsonChildValue(object, child_name, ov::Converter::ToInt32(original_value));
				break;

			case ValueType::Long:
				SetJsonChildValue(object, child_name, ov::Converter::ToInt64(original_value));
				break;

			case ValueType::Boolean:
				SetJsonChildValue(object, child_name, ov::Converter::ToBool(original_value));
				break;

			case ValueType::Double:
				SetJsonChildValue(object, child_name, ov::Converter::ToDouble(original_value));
				break;

			case ValueType::Attribute:
				SetJsonChildValue(object["$"], child_name, original_value);
				break;

			case ValueType::Text: {
				if (object.isObject() == false)
				{
					object = Json::objectValue;
				}
				SetJsonChildValue(object, child_name, original_value);
				break;
			}

			case ValueType::Item: {
				try
				{
					auto item_json = TryCast<Item *>(child_target)->ToJsonInternal(include_default_values);
					SetJsonChildValue(object, child_name, item_json);
				}
				catch (const CastException &cast_exception)
				{
					throw CreateConfigError("Could not cast %s to %s", cast_exception.from.CStr(), cast_exception.to.CStr());
				}
				break;
			}

			case ValueType::List: {
				std::shared_ptr<ListInterface> list_item;

				try
				{
					list_item = TryCast<std::shared_ptr<ListInterface>>(child_target);
				}
				catch (const CastException &cast_exception)
				{
					throw CreateConfigError("Could not cast %s to %s", cast_exception.from.CStr(), cast_exception.to.CStr());
				}

				auto &list_children = list_item->GetChildren();
				auto list_value_type = list_item->GetValueType();

				auto &list_item_name = list_item->GetItemName();

				Json::Value &target_object = (omit_rule == OmitRule::DontOmit) ? object[list_item_name.json_name] : object;

				if (target_object.isArray() == false)
				{
					target_object = Json::arrayValue;
				}

				for (auto &list_child : list_children)
				{
					AddJsonChild(target_object, list_value_type, list_item_name.omit_rule, list_child->GetItemName().json_name, list_child->GetTarget(), list_child->GetOriginalValue(), include_default_values);
				}

				break;
			}
		}
	}

	void SetXmlChildValue(pugi::xml_node &node, const ov::String &child_name, const char *value)
	{
		auto child_node = node.append_child(child_name.CStr());
		child_node.set_name(child_name.CStr());

		auto child_text = child_node.append_child(pugi::node_pcdata);
		child_text.set_value(value);
	}

	void Item::AddXmlChild(pugi::xml_node &node, ValueType value_type, const ov::String &child_name, const std::any &child_target, const Json::Value &original_value, bool include_default_values)
	{
		switch (value_type)
		{
			case ValueType::Unknown:
				node.append_child(pugi::node_null).set_name(child_name.CStr());
				break;

			case ValueType::String:
				SetXmlChildValue(node, child_name, StringFromJsonValue(original_value));
				break;

			case ValueType::Integer:
				SetXmlChildValue(node, child_name, ov::Converter::ToString(ov::Converter::ToInt32(original_value)));
				break;

			case ValueType::Long:
				SetXmlChildValue(node, child_name, ov::Converter::ToString(ov::Converter::ToInt64(original_value)));
				break;

			case ValueType::Boolean:
				SetXmlChildValue(node, child_name, ov::Converter::ToBool(original_value) ? "true" : "false");
				break;

			case ValueType::Double:
				SetXmlChildValue(node, child_name, ov::Converter::ToString(ov::Converter::ToDouble(original_value)));
				break;

			case ValueType::Attribute:
				node.append_attribute(child_name).set_value(StringFromJsonValue(original_value));
				break;

			case ValueType::Text:
				SetXmlChildValue(node, child_name, StringFromJsonValue(original_value));
				break;

			case ValueType::Item: {
				try
				{
					auto child_node = node.append_child(child_name.CStr());
					TryCast<Item *>(child_target)->ToXmlInternal(child_node, include_default_values);
				}
				catch (const CastException &cast_exception)
				{
					throw CreateConfigError("Could not cast %s to %s", cast_exception.from.CStr(), cast_exception.to.CStr());
				}
				break;
			}

			case ValueType::List: {
				std::shared_ptr<ListInterface> list_item;

				try
				{
					list_item = TryCast<std::shared_ptr<ListInterface>>(child_target);
				}
				catch (const CastException &cast_exception)
				{
					throw CreateConfigError("Could not cast %s to %s", cast_exception.from.CStr(), cast_exception.to.CStr());
				}

				auto &list_children = list_item->GetChildren();
				auto list_value_type = list_item->GetValueType();

				auto &list_item_name = list_item->GetItemName();

				for (auto &list_child : list_children)
				{
					AddXmlChild(node, list_value_type, list_item_name.xml_name, list_child->GetTarget(), list_child->GetOriginalValue(), include_default_values);
				}
				break;
			}
		}
	}

	Json::Value Item::ToJson(bool include_default_values) const
	{
		return ToJsonInternal(include_default_values);
	}

	Json::Value Item::ToJsonInternal(bool include_default_values) const
	{
		RebuildListIfNeeded();

		Json::Value object = Json::objectValue;

		for (auto &child : _children)
		{
			if (include_default_values || child->IsParsed())
			{
				AddJsonChild(object, child->GetType(), _item_name.omit_rule, child->GetName().json_name, child->GetTarget(), child->GetOriginalValue(), include_default_values);
			}
		}

		return object;
	}

	void Item::ToXml(pugi::xml_node node, bool include_default_values) const
	{
		if (node.set_name(_item_name.xml_name.CStr()))
		{
			ToXmlInternal(node, include_default_values);
		}
	}

	pugi::xml_document Item::ToXml(bool include_default_values) const
	{
		pugi::xml_document doc;

		auto root_node = doc.append_child(_item_name.xml_name.CStr());

		ToXmlInternal(root_node, include_default_values);

		return doc;
	}

	void Item::ToXmlInternal(pugi::xml_node &parent_node, bool include_default_values) const
	{
		RebuildListIfNeeded();

		for (auto &child : _children)
		{
			if (include_default_values || child->IsParsed())
			{
				AddXmlChild(parent_node, child->GetType(), child->GetName().xml_name, child->GetTarget(), child->GetOriginalValue(), include_default_values);
			}
		}
	}

	bool Item::SetValue(const std::shared_ptr<const Child> &child, ValueType type, std::any &child_target, const ov::String &item_path, const ItemName &child_name, const ov::String &name, const std::any &value)
	{
		ov::String child_path = ov::String::FormatString(
			"%s%s%s",
			item_path.CStr(),
			item_path.IsEmpty() ? "" : ".",
			name.CStr());

		if (value.has_value() == false)
		{
			if (child->IsOptional() == false)
			{
				throw CreateConfigError("%s is not optional", child_path.CStr());
			}
			else
			{
				auto error = child->CallOptionalCallback();

				if (error != nullptr)
				{
					throw error;
				}

				// Callback returns no error. Use default value
			}

			return false;
		}

		try
		{
			switch (type)
			{
				case ValueType::Unknown:
					logtd("[%s] Unknown type: %s",
						  item_path.CStr(),
						  child_name.ToString().CStr());
					break;

				case ValueType::String:
					logtd("[%s] Trying to cast %s for %s",
						  item_path.CStr(),
						  ov::Demangle(value.type().name()).CStr(),
						  child_name.ToString().CStr());
					*(std::any_cast<ov::String *>(child_target)) = TryCast<ov::String, ov::String, const char *>(value);
					break;

				case ValueType::Integer:
					logtd("[%s] Trying to cast %s for %s",
						  item_path.CStr(),
						  ov::Demangle(value.type().name()).CStr(),
						  child_name.ToString().CStr());
					*(std::any_cast<int *>(child_target)) = TryCast<int, int, long, int64_t>(value);
					break;

				case ValueType::Long:
					logtd("[%s] Trying to cast %s for %s",
						  item_path.CStr(),
						  ov::Demangle(value.type().name()).CStr(),
						  child_name.ToString().CStr());
					*(std::any_cast<int64_t *>(child_target)) = TryCast<int, int64_t, long, int>(value);
					break;

				case ValueType::Boolean:
					logtd("[%s] Trying to cast %s for %s",
						  item_path.CStr(),
						  ov::Demangle(value.type().name()).CStr(),
						  child_name.ToString().CStr());
					*(std::any_cast<bool *>(child_target)) = TryCast<bool, bool, int>(value);
					break;

				case ValueType::Double:
					logtd("[%s] Trying to cast %s for %s",
						  item_path.CStr(),
						  ov::Demangle(value.type().name()).CStr(),
						  child_name.ToString().CStr());
					*(std::any_cast<double *>(child_target)) = TryCast<double, double, float>(value);
					break;

				case ValueType::Attribute:
					logtd("[%s] Trying to cast %s for %s (attribute)",
						  item_path.CStr(),
						  ov::Demangle(value.type().name()).CStr(),
						  child_name.ToString().CStr());
					std::any_cast<Attribute *>(child_target)->_value = TryCast<ov::String, ov::String, const char *>(value);
					break;

				case ValueType::Text:
					logtd("[%s] Trying to cast %s for %s(text)",
						  item_path.CStr(),
						  ov::Demangle(value.type().name()).CStr(),
						  child_name.ToString().CStr());
					std::any_cast<Text *>(child_target)->FromString(TryCast<ov::String, ov::String, const char *>(value));
					break;

				case ValueType::Item:
					logtd("[%s] Trying to cast %s for %s",
						  item_path.CStr(),
						  ov::Demangle(value.type().name()).CStr(),
						  child_name.ToString().CStr());
					std::any_cast<Item *>(child_target)->FromDataSource(child_path, child_name, TryCast<const cfg::DataSource &>(value));
					break;

				case ValueType::List: {
					logtd("[%s] Trying to cast %s for %s",
						  item_path.CStr(),
						  ov::Demangle(value.type().name()).CStr(),
						  child_name.ToString().CStr());

					auto &list_target = std::any_cast<std::shared_ptr<ListInterface> &>(child_target);

					const std::vector<cfg::DataSource> &data_sources = TryCast<const std::vector<cfg::DataSource> &>(value);

					std::vector<DataSource> new_data_sources;

					for (auto &data_source : data_sources)
					{
						if (data_source.IsSourceOf(child_name))
						{
							// Check the child has an include attribute
							std::vector<DataSource> data_sources_for_array;

							{
								std::vector<ov::String> include_files;

								if (GetIncludeFileList(data_source, nullptr, &include_files))
								{
									for (auto &include_file : include_files)
									{
										data_sources_for_array.push_back(data_source.NewDataSource(include_file, child_name));
									}
								}
								else
								{
									// "include" attribute is not present
									data_sources_for_array.push_back(data_source);
								}
							}

							if (data_source.GetType() == DataType::Json)
							{
								for (auto &data_source_for_array : data_sources_for_array)
								{
									if (data_source_for_array.IsArray(child_name))
									{
										// data_source_for_array contains multiple data (eg: [item1, item2, ...])
										auto list = data_source_for_array.GetRootValue(ValueType::List, child->ResolvePath(), child->_name.omit_rule, nullptr);

										if (list.has_value())
										{
											auto data_source_list = TryCast<const std::vector<cfg::DataSource> &>(list);

											for (auto &source_from_file : data_source_list)
											{
												new_data_sources.push_back(source_from_file);
											}
										}
										else
										{
											// empty list
										}
									}
									else
									{
										if (child_name.omit_rule == OmitRule::DontOmit)
										{
											throw CreateConfigError("%s is not an object", child_path.CStr());
										}
										else
										{
											new_data_sources.push_back(data_source_for_array);
										}
									}
								}
							}
							else
							{
								for (auto &data_source_for_array : data_sources_for_array)
								{
									new_data_sources.push_back(data_source_for_array);
								}
							}
						}
						else
						{
							// The parent has multiple types of items
						}
					}

					list_target->Clear();
					list_target->Allocate(new_data_sources.size());

					size_t index = 0;

					list_target->SetItemName(child_name);

					ItemName new_name = child_name;

					for (auto &new_data_source : new_data_sources)
					{
						Json::Value original_value;
						auto list_value = new_data_source.GetRootValue(list_target->GetValueType(), child->ResolvePath(), child->_name.omit_rule, &original_value);

						if (list_value.has_value() == false)
						{
							continue;
						}

						new_name.index = index;

						auto new_item = list_target->GetAt(index);

						auto list_child = std::make_shared<ListChild>(new_name, new_item, original_value);
						list_target->AddListChild(list_child);

						SetValue(child, list_target->GetValueType(), new_item, item_path, child_name, name, list_value);

						index++;
					}
				}

				break;
			}
		}
		catch (const CastException &cast_exception)
		{
			throw CreateConfigError(
				"[%s] Could not convert value: type: %s, expected: %s, but: %s",
				item_path.CStr(),
				StringFromValueType(child->GetType()),
				cast_exception.to.CStr(),
				cast_exception.from.CStr());
		}

		auto validation_error = child->CallValidationCallback();
		if (validation_error != nullptr)
		{
			throw validation_error;
		}

		return true;
	}

	void Item::FromDataSourceInternal(ov::String item_path, const DataSource &data_source)
	{
		if (_need_to_update_list)
		{
			throw CreateConfigError(
				"Rebuilding of children list is required before call FromDataSource()");
		}

		ov::String pattern;
		std::vector<ov::String> include_files;

		if (GetIncludeFileList(data_source, &pattern, &include_files))
		{
			auto current_path = data_source.GetCurrentPath();

			if (include_files.empty())
			{
				// "include" attribute is present, but there is no file to include
				throw CreateConfigError(
					"There is no file to include for path: %s, current path: %s, include pattern: %s",
					item_path.CStr(),
					current_path.CStr(), pattern.CStr());
			}
			else
			{
				if (include_files.size() > 1)
				{
					throw CreateConfigError(
						"Too many files found for an Item: %s, current path: %s, include pattern: %s (%zu files found)",
						item_path.CStr(),
						current_path.CStr(), pattern.CStr(), include_files.size());
				}

				auto &include_file = include_files[0];
				auto new_data_source = data_source.NewDataSource(include_file, _item_name);
				FromDataSourceInternal(item_path, new_data_source);
			}

			return;
		}
		else
		{
			// "include" attribute is not present
		}

		for (auto &child : _children)
		{
			Json::Value original_value;
			auto &child_name = child->GetName();

			auto value = data_source.GetValue(child->GetType(), child_name, child->ResolvePath(), _item_name.omit_rule, &original_value);
			auto name = data_source.ResolveName(child_name);
			auto &child_target = child->GetTarget();

			child->_is_parsed = SetValue(child, child->GetType(), child_target, item_path, child_name, name, value);
			child->SetOriginalValue(original_value);
		}

		_is_parsed = true;
	}
}  // namespace cfg
