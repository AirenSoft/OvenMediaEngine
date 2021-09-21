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

#include "config_error.h"

namespace cfg
{
	class Item;

	class ListInterface
	{
		friend class Item;

	public:
		ListInterface(ValueType type)
			: _type(type)
		{
		}

		virtual ~ListInterface() = default;

		virtual void Allocate(size_t size) = 0;
		virtual std::any GetAt(size_t index) = 0;

		virtual ov::String ToString(int indent_count, const std::shared_ptr<Child> &child) const = 0;

		size_t GetCount() const
		{
			return _list_children.size();
		}

		virtual void Clear()
		{
			_list_children.clear();
		}

		ValueType GetValueType() const
		{
			return _type;
		}

		void SetItemName(const ItemName &item_name)
		{
			_item_name = item_name;
		}

		const ItemName &GetItemName() const
		{
			return _item_name;
		}

		void AddListChild(const std::shared_ptr<ListChild> &list_child)
		{
			_list_children.push_back(list_child);
		}

		virtual void CopyChildrenFrom(const std::any &another_list) = 0;

		const std::vector<std::shared_ptr<ListChild>> &GetChildren() const
		{
			return _list_children;
		}

		// Get type name of Ttype
		virtual ov::String GetTypeName() const = 0;

	protected:
		MAY_THROWS(std::shared_ptr<ConfigError>)
		void ValidateOmitRule(const ov::String &path) const
		{
			auto value_type = GetValueType();

			switch (value_type)
			{
				case ValueType::Unknown:
					[[fallthrough]];
				case ValueType::String:
					[[fallthrough]];
				case ValueType::Integer:
					[[fallthrough]];
				case ValueType::Long:
					[[fallthrough]];
				case ValueType::Boolean:
					[[fallthrough]];
				case ValueType::Double:
					[[fallthrough]];
				case ValueType::Attribute:
					[[fallthrough]];
				case ValueType::Text:
					OV_ASSERT(value_type == ValueType::List, "ListInterface should contains Item or List type, but: %s", StringFromValueType(value_type));
					throw CreateConfigError("Internal config module error");

				case ValueType::Item:
					[[fallthrough]];
				// Since ListInterface does not know exactly the types of sub-items at this point, sub-class is responsible for validation.
				case ValueType::List:
					ValidateOmitRuleInternal(path);
			}
		}

		MAY_THROWS(std::shared_ptr<ConfigError>)
		virtual void ValidateOmitRuleInternal(const ov::String &path) const = 0;

		ValueType _type;
		ItemName _item_name;

		std::vector<std::shared_ptr<ListChild>> _list_children;
	};
}  // namespace cfg
