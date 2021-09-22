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
#include <functional>

#include "./annotations.h"
#include "./attribute.h"
#include "./config_error.h"
#include "./data_source.h"
#include "./declare_utilities.h"

// Value types
#include "./child.h"
#include "./list_child.h"
#include "./list_interface.h"
#include "./text.h"
#include "./value_type.h"

#define CFG_VERBOSE_STRING 1

#if CFG_VERBOSE_STRING
#	define CFG_EXTRA_PREFIX "                // <"
#	define CFG_EXTRA_SUFFIX ">"
#else  // CFG_VERBOSE_STRING
#	define CFG_EXTRA_PREFIX ""
#	define CFG_EXTRA_SUFFIX ""
#endif	// CFG_VERBOSE_STRING

namespace cfg
{
	class Item
	{
	protected:
		static ov::String ChildToString(int indent_count, const std::shared_ptr<Child> &child, size_t index, size_t child_count);

		template <typename Ttype, std::enable_if_t<!std::is_base_of_v<Item, Ttype>, int> = 0>
		static std::any MakeAny(Ttype *item)
		{
			return item;
		}

		template <typename Ttype, std::enable_if_t<std::is_base_of_v<Item, Ttype>, int> = 0>
		static std::any MakeAny(Ttype *item)
		{
			return static_cast<Item *>(item);
		}

		template <typename Ttype>
		class List : public ListInterface
		{
		public:
			using ListType = std::vector<Ttype>;

			List(ListType *target)
				: ListInterface(ProbeType<Ttype>::type),
				  _target(target)
			{
			}

			void CopyChildrenFrom(const std::any &another_list) override
			{
				auto from = std::static_pointer_cast<List<Ttype>>(TryCast<std::shared_ptr<ListInterface>>(another_list));

				_item_name = from->_item_name;

				_list_children = from->_list_children;

				// _target points to the Item's value;
				auto iterator = _target->begin();

				// Rebuild target list
				for (auto &list_child : _list_children)
				{
					list_child->Update(MakeAny(&(*iterator)));
					iterator++;
				}
			}

			ov::String GetTypeName() const override
			{
				return ov::Demangle(typeid(Ttype).name());
			}

			void Allocate(size_t size) override
			{
				_target->resize(size);
			}

			std::any GetAt(size_t index) override
			{
				return MakeAny(&(_target->operator[](index)));
			}

			void Clear() override
			{
				ListInterface::Clear();
				_target->clear();
			}

			template <typename Tvalue_type, std::enable_if_t<!std::is_base_of_v<Item, Tvalue_type>, int> = 0>
			ov::String ConvertToString(int indent_count, const Tvalue_type *value) const
			{
				return cfg::ToString(value);
			}

			ov::String ConvertToString(int indent_count, const Item *value) const
			{
				return cfg::ToString(indent_count, value);
			}

			ov::String ToString(int indent_count, const std::shared_ptr<Child> &list_info) const override
			{
				size_t index = 0;
				size_t child_count = _target->size();

				if (child_count == 0)
				{
					return "";
				}

				ov::String indent = MakeIndentString(indent_count);

				ov::String description;

				for (auto &item : *_target)
				{
					ov::String comma = (index < (child_count - 1)) ? ", " : "";

					if (GetValueType() == ValueType::Item)
					{
						description.AppendFormat(
							"%s"
							"%s"
							"\n",
							ConvertToString(indent_count, &item).CStr(),
							comma.CStr());
					}
					else
					{
						description.AppendFormat(
							"%s"
							"%zu: %s"
							"%s"
							"\n",
							indent.CStr(),
							index, ConvertToString(indent_count + 1, &item).CStr(),
							comma.CStr());
					}

					index++;
				}

				return description;
			}

		protected:
			MAY_THROWS(std::shared_ptr<ConfigError>)
			void ValidateOmitRuleInternal(const ov::String &path) const override
			{
				switch (GetValueType())
				{
					case ValueType::Item: {
						ValidateOmitRuleForItem(path);
						break;
					}

					case ValueType::List: {
						// Config module doesn't support nested list
						throw CreateConfigError("Config module doesn't support nested list");
					}

					default:
						// Do not need to validate omit rule for other types
						break;
				}
			}

			ListType *_target;

		private:
			MAY_THROWS(std::shared_ptr<ConfigError>)
			template <typename Titem_type = Ttype, std::enable_if_t<std::is_base_of_v<Item, Titem_type>, int> = 0>
			void ValidateOmitRuleForItem(const ov::String &path) const
			{
				Ttype item;

				item.ValidateOmitRules(path);
			}

			MAY_THROWS(std::shared_ptr<ConfigError>)
			template <typename Titem_type = Ttype, std::enable_if_t<!std::is_base_of_v<Item, Titem_type>, int> = 0>
			void ValidateOmitRuleForItem(const ov::String &path) const
			{
			}
		};

	public:
		Item() = default;

		Item(const Item &item);
		Item(Item &&item);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void FromDataSource(ov::String path, const ItemName &name, const DataSource &data_source);

		Item &operator=(const Item &item);

		bool IsDefault() const
		{
			return _is_parsed == false;
		}

		bool IsParsed() const
		{
			return _is_parsed;
		}

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void ValidateOmitRules(const ov::String &path) const;

		ov::String ToString() const
		{
			return ToString(0);
		}

		ov::String ToString(int indent_count) const;

		Json::Value ToJson(bool include_default_values = false) const;
		void ToXml(pugi::xml_node node, bool include_default_values = false) const;
		pugi::xml_document ToXml(bool include_default_values = false) const;

	protected:
		// Returns true if the value exists, otherwise returns false
		MAY_THROWS(std::shared_ptr<ConfigError>)
		static bool SetValue(const std::shared_ptr<const Child> &child, ValueType type, std::any &child_target, const ov::String &path, const ItemName &child_name, const ov::String &name, const std::any &value);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void FromDataSourceInternal(ov::String path, const DataSource &data_source);

		bool IsParsed(const void *target) const;

		// When converting to JSON, check if the sub-items of this item can be omitted.
		// If an error is thrown when call this function, there is a misdeveloped config.
		//
		// For example, the following cases may be omitted:
		//
		// <Items>
		//     <Item>Hello</Item>
		//     <Item>World</Item>
		// </Items>
		// =>
		// { "items": [ "Hello", "World" ] }
		//
		// But, the following cases cannot be omitted:
		//
		// <Items>
		//     <Item>Hello</Item>
		//     <Item>World</Item>
		//     <AnotherItem />
		// </Items>
		MAY_THROWS(std::shared_ptr<ConfigError>)
		void ValidateOmitRule(const ov::String &path, const ov::String &name) const;

		void RebuildListIfNeeded() const;
		void RebuildListIfNeeded();

		virtual void MakeList() = 0;

		void AddChild(const ItemName &name, ValueType type, const ov::String &type_name,
					  bool is_optional, bool resolve_path,
					  OptionalCallback optional_callback, ValidationCallback validation_callback,
					  const void *raw_target, std::any target);

		// For primitive types
		template <
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void,
			typename Ttype, std::enable_if_t<!std::is_base_of_v<Item, Ttype> && !std::is_base_of_v<Text, Ttype>, int> = 0>
		void Register(const ItemName &name, Ttype *value, OptionalCallback optional_callback = nullptr, ValidationCallback validation_callback = nullptr)
		{
			AddChild(name, ProbeType<Ttype>::type, ov::Demangle(typeid(Ttype).name()),
					 CheckAnnotations<Optional, Tannotation1, Tannotation2, Tannotation3>::value,
					 CheckAnnotations<ResolvePath, Tannotation1, Tannotation2, Tannotation3>::value,
					 optional_callback, validation_callback,
					 value, value);
		}

		// For Text
		template <
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void,
			typename Ttype, std::enable_if_t<std::is_base_of_v<Text, Ttype>, int> = 0>
		void Register(const ItemName &name, Ttype *value, OptionalCallback optional_callback = nullptr, ValidationCallback validation_callback = nullptr)
		{
			AddChild(name, ProbeType<Ttype>::type, ov::Demangle(typeid(Ttype).name()),
					 CheckAnnotations<Optional, Tannotation1, Tannotation2, Tannotation3>::value,
					 CheckAnnotations<ResolvePath, Tannotation1, Tannotation2, Tannotation3>::value,
					 optional_callback, validation_callback,
					 value, static_cast<Text *>(value));
		}

		// For Item
		template <
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void,
			typename Ttype, std::enable_if_t<std::is_base_of_v<Item, Ttype>, int> = 0>
		void Register(const ItemName &name, Ttype *value, OptionalCallback optional_callback = nullptr, ValidationCallback validation_callback = nullptr)
		{
			AddChild(name, ProbeType<Ttype>::type, ov::Demangle(typeid(Ttype).name()),
					 CheckAnnotations<Optional, Tannotation1, Tannotation2, Tannotation3>::value,
					 CheckAnnotations<ResolvePath, Tannotation1, Tannotation2, Tannotation3>::value,
					 optional_callback, validation_callback,
					 value, static_cast<Item *>(value));

			value->_item_name = name;
		}

		// For std::vector<Ttype>
		template <
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void,
			typename Ttype>
		void Register(const ItemName &name, std::vector<Ttype> *value, OptionalCallback optional_callback = nullptr, ValidationCallback validation_callback = nullptr)
		{
			auto list = std::make_shared<List<Ttype>>(value);

			AddChild(name, ValueType::List, ov::Demangle(typeid(Ttype).name()),
					 CheckAnnotations<Optional, Tannotation1, Tannotation2, Tannotation3>::value,
					 CheckAnnotations<ResolvePath, Tannotation1, Tannotation2, Tannotation3>::value,
					 optional_callback, validation_callback,
					 value, std::static_pointer_cast<ListInterface>(list));
		}

		static void AddJsonChild(Json::Value &object, ValueType value_type, OmitRule omit_name, const ov::String &child_name, const std::any &child_target, const Json::Value &original_value, bool include_default_values);
		static void AddXmlChild(pugi::xml_node &node, ValueType value_type, const ov::String &child_name, const std::any &child_target, const Json::Value &original_value, bool include_default_values);

		Json::Value ToJsonInternal(bool include_default_values) const;
		void ToXmlInternal(pugi::xml_node &parent_node, bool include_default_values) const;

		bool _need_to_update_list = true;

		bool _is_parsed = false;

		ItemName _item_name;

		std::map<ov::String, std::shared_ptr<Child>> _children_for_xml;
		std::map<ov::String, std::shared_ptr<Child>> _children_for_json;

		std::vector<std::shared_ptr<Child>> _children;
	};
}  // namespace cfg
