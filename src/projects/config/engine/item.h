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

#include "annotations.h"
#include "config_error.h"
#include "data_source.h"
#include "value_type.h"

#define CFG_VERBOSE_STRING 1

#if CFG_VERBOSE_STRING
#	define CFG_EXTRA_PREFIX "                // <"
#	define CFG_EXTRA_SUFFIX ">"
#else  // CFG_VERBOSE_STRING
#	define CFG_EXTRA_PREFIX ""
#	define CFG_EXTRA_SUFFIX ""
#endif	// CFG_VERBOSE_STRING

// const auto &GetInt() const { return int_value; }
#define CFG_DECLARE_REF_GETTER_OF(getter_name, variable_name) \
	const auto &getter_name(bool *is_parsed = nullptr) const  \
	{                                                         \
		if (is_parsed != nullptr)                             \
		{                                                     \
			*is_parsed = IsParsed(&variable_name);            \
		}                                                     \
                                                              \
		return variable_name;                                 \
	}

// virtual const decltype(int_value) GetInt() const { return int_value; }
#define CFG_DECLARE_VIRTUAL_REF_GETTER_OF(getter_name, variable_name)                   \
	virtual const decltype(variable_name) &getter_name(bool *is_parsed = nullptr) const \
	{                                                                                   \
		if (is_parsed != nullptr)                                                       \
		{                                                                               \
			*is_parsed = IsParsed(&variable_name);                                      \
		}                                                                               \
                                                                                        \
		return variable_name;                                                           \
	}

// decltype(int_value) GetInt() const override { return int_value; }
#define CFG_DECLARE_OVERRIDED_GETTER_OF(getter_name, variable_name)               \
	decltype(variable_name) getter_name(bool *is_parsed = nullptr) const override \
	{                                                                             \
		if (is_parsed != nullptr)                                                 \
		{                                                                         \
			*is_parsed = IsParsed(&variable_name);                                \
		}                                                                         \
                                                                                  \
		return variable_name;                                                     \
	}

namespace cfg
{
	class Item;

	// A callback called to determine if the value is conditional optional
	using OptionalCallback = std::function<std::shared_ptr<ConfigError>()>;

	using ValidationCallback = std::function<std::shared_ptr<ConfigError>()>;

	class Child
	{
		friend class Item;

	public:
		Child() = default;
		Child(const ItemName &name, ValueType type, const ov::String &type_name,
			  bool is_optional, bool resolve_path,
			  OptionalCallback optional_callback, ValidationCallback validation_callback,
			  const void *raw_target, std::any target)
			: _name(name),
			  _type(type),
			  _type_name(type_name),

			  _is_optional(is_optional),
			  _resolve_path(resolve_path),

			  _optional_callback(optional_callback),
			  _validation_callback(validation_callback),

			  _raw_target(raw_target),
			  _target(std::move(target))
		{
		}

		bool IsParsed() const
		{
			return _is_parsed;
		}

		const ItemName &GetName() const
		{
			return _name;
		}

		ValueType GetType() const
		{
			return _type;
		}

		const ov::String &GetTypeName() const
		{
			return _type_name;
		}

		bool IsOptional() const
		{
			return _is_optional;
		}

		bool ResolvePath() const
		{
			return _resolve_path;
		}

		OptionalCallback GetOptionalCallback() const
		{
			return _optional_callback;
		}

		ValidationCallback GetValidationCallback() const
		{
			return _validation_callback;
		}

		std::shared_ptr<ConfigError> CallOptionalCallback() const
		{
			if (_optional_callback != nullptr)
			{
				return _optional_callback();
			}

			return nullptr;
		}

		std::shared_ptr<ConfigError> CallValidationCallback() const
		{
			if (_validation_callback != nullptr)
			{
				return _validation_callback();
			}

			return nullptr;
		}

		const void *GetRawTarget() const
		{
			return _raw_target;
		}

		std::any &GetTarget()
		{
			return _target;
		}

		const std::any &GetTarget() const
		{
			return _target;
		}

		const Json::Value &GetOriginalValue() const
		{
			return _original_value;
		}

	protected:
		void SetOriginalValue(Json::Value value)
		{
			_original_value = std::move(value);
		}

		void Update(
			OptionalCallback optional_callback, ValidationCallback validation_callback,
			const void *raw_target, std::any target)
		{
			_optional_callback = optional_callback;
			_validation_callback = validation_callback;
			_raw_target = raw_target;
			_target = std::move(target);
		}

		bool _is_parsed = false;

		ItemName _name;
		ValueType _type = ValueType::Unknown;
		ov::String _type_name;

		bool _is_optional = false;
		bool _resolve_path = false;

		OptionalCallback _optional_callback = nullptr;
		ValidationCallback _validation_callback = nullptr;

		// This value used to determine a member using a pointer
		const void *_raw_target = nullptr;

		// Contains a pointer of member variable
		std::any _target;

		Json::Value _original_value;
	};

	class ListChild
	{
	public:
		ListChild(
			const ItemName &item_name,
			std::any &target,
			const Json::Value &original_value)
			: _item_name(item_name),
			  _target(target),
			  _original_value(original_value)
		{
		}

		const ItemName &GetItemName() const
		{
			return _item_name;
		}

		std::any &GetTarget()
		{
			return _target;
		}

		const Json::Value &GetOriginalValue() const
		{
			return _original_value;
		}

		void Update(std::any target)
		{
			_target = target;
		}

	protected:
		ItemName _item_name;
		std::any _target;
		Json::Value _original_value;
	};

	class Text
	{
	protected:
		friend class Item;

	public:
		virtual ov::String ToString() const = 0;

	protected:
		MAY_THROWS(std::shared_ptr<ConfigError>)
		virtual void FromString(const ov::String &str) = 0;

		bool IsParsed(const void *target) const
		{
			throw CreateConfigError("Use Item::IsParsed() instead");
		}
	};

	class Attribute
	{
	protected:
		friend class Item;

	public:
		const ov::String &GetValue() const
		{
			return _value;
		}

		operator const char *() const
		{
			return _value;
		}

	protected:
		ov::String _value;
	};

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
		ValueType _type;
		ItemName _item_name;

		std::vector<std::shared_ptr<ListChild>> _list_children;
	};

	ov::String ChildToString(int indent_count, const std::shared_ptr<Child> &child, size_t index, size_t child_count);

	template <typename Ttype, std::enable_if_t<!std::is_base_of_v<Item, Ttype>, int> = 0>
	static std::any MakeAny(Ttype *item)
	{
		return item;
	}

	template <typename Ttype, std::enable_if_t<std::is_base_of_v<Item, Ttype>, int> = 0>
	std::any MakeAny(Ttype *item)
	{
		return static_cast<Item *>(item);
	}

	class Item
	{
	protected:
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
			ListType *_target;
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

		ov::String ToString() const
		{
			return ToString(0);
		}

		ov::String ToString(int indent_count) const;

		Json::Value ToJson(bool include_default_values = false) const;

	protected:
		// Returns true if the value exists, otherwise returns false
		MAY_THROWS(std::shared_ptr<ConfigError>)
		static bool SetValue(const std::shared_ptr<const Child> &child, ValueType type, std::any &child_target, const ov::String &path, const ItemName &child_name, const ov::String &name, const std::any &value);

		MAY_THROWS(std::shared_ptr<ConfigError>)
		void FromDataSourceInternal(ov::String path, const DataSource &data_source);

		bool IsParsed(const void *target) const;

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

		static void AddJsonChild(Json::Value &object, ValueType value_type, const ov::String &child_name, const std::any &child_target, const Json::Value &original_value, bool include_default_values);

		Json::Value ToJsonInternal(bool include_default_values) const;

		bool _need_to_update_list = true;

		bool _is_parsed = false;

		ItemName _item_name;

		std::map<ov::String, std::shared_ptr<Child>> _children_for_xml;
		std::map<ov::String, std::shared_ptr<Child>> _children_for_json;

		std::vector<std::shared_ptr<Child>> _children;
	};
}  // namespace cfg
