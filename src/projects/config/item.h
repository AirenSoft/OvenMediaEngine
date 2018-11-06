//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "value_for_int.h"
#include "value_for_bool.h"
#include "value_for_float.h"
#include "value_for_string.h"
#include "value_for_item.h"
#include "value_for_list.h"

namespace cfg
{
	// Annotations
	struct Optional;
	struct Includable;
	struct Overridable;

	class Item
	{
	public:
		friend class ValueBase;

		Item() = default;
		virtual ~Item() = default;

		void SetParent(Item *parent);
		Item *GetParent();
		const Item *GetParent() const;

		bool IsValid() const;

		virtual bool Parse(const ov::String &file_name, const ov::String &tag_name);

		virtual ov::String ToString() const;
		virtual ov::String ToString(int indent) const;

	protected:
		// Annotation Utilities
		template<typename... Args>
		struct CheckAnnotations;

		template<typename Texpected, typename Ttype, typename... Ttypes>
		struct CheckAnnotations<Texpected, Ttype, Ttypes...>
		{
			static constexpr bool value = std::is_same<Texpected, Ttype>::value || CheckAnnotations<Texpected, Ttypes...>::value;
		};

		template<typename Texpected>
		struct CheckAnnotations<Texpected>
		{
			static constexpr bool value = false;
		};

		// ValueType + Up to 5 annotations
		template<
			ValueType config_type,
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void, typename Tannotation4 = void, typename Tannotation5 = void,
			typename T
		>
		bool RegisterValue(const ov::String &name, Value<T> *value, ValueType type = config_type)
		{
			bool is_optional = CheckAnnotations<Optional, Tannotation1, Tannotation2, Tannotation3, Tannotation4, Tannotation5>::value;
			bool is_includable = CheckAnnotations<Includable, Tannotation1, Tannotation2, Tannotation3, Tannotation4, Tannotation5>::value;
			bool is_overridable = CheckAnnotations<Overridable, Tannotation1, Tannotation2, Tannotation3, Tannotation4, Tannotation5>::value;

			if(config_type != ValueType::Unknown)
			{
				value->SetType(config_type);
			}

			value->SetParent(this);
			value->SetName(name);
			value->SetOptional(is_optional);
			value->SetIncludable(is_includable);
			value->SetOverridable(is_overridable);

			_parse_list[name] = value;

			return true;
		}

		// Up to 5 annotations
		template<
			typename Tannotation1 = void, typename Tannotation2 = void, typename Tannotation3 = void, typename Tannotation4 = void, typename Tannotation5 = void,
			typename T
		>
		bool RegisterValue(const ov::String &name, Value<T> *value)
		{
			return RegisterValue<ValueType::Unknown, Tannotation1, Tannotation2, Tannotation3, Tannotation4, Tannotation5, T>(name, value);
		}

		ValueBase *FindParentValue(const ValueBase *value) const;

		// parse 하기 직전에 한 번 호출해주면 됨
		virtual bool MakeParseList() = 0;

		// element안에 있는 include attribute를 처리함
		virtual bool PreProcess(const ov::String &tag_name, const pugi::xml_node &node, const ValueBase *value_type, int indent);
		virtual bool ParseFromFile(const ov::String &file_name, const ov::String &tag_name, bool processing_include_file, int indent);

		virtual bool ParseFromNode(const pugi::xml_node &node, const ov::String &tag_name, int indent);
		virtual bool ParseFromNode(const pugi::xml_node &node, const ov::String &tag_name, bool processing_include_file, int indent);

		virtual bool PostProcess(int indent);

		Item *_parent = nullptr;
		ov::String _tag_name;

		std::map<ov::String, ValueBase *> _parse_list;
	};
};