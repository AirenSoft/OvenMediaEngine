//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>

#include <base/ovlibrary/ovlibrary.h>
#include <pugixml-1.9/src/pugixml.hpp>

namespace cfg
{
	enum class ValueType
	{
		Unknown = 'U',
		// A text of the child element
			String = 'S',
		// An integer value of the child element
			Integer = 'I',
		// A boolean value of the child element
			Boolean = 'B',
		// A float value of the child element
			Float = 'F',
		// An element of the child element
			Element = 'E',
		// An element list of the child element
			List = 'L',
		// An attribute of the element
			Attribute = 'A',
		// A text of the element
			Text = 'T'
	};

	class Item;

	class ValueBase
	{
	public:
		ValueBase() = default;
		virtual ~ValueBase() = default;

		ValueType GetType() const;
		void SetType(ValueType type);

		ov::String GetName() const;
		void SetName(const ov::String &name);

		const Item *GetParent() const;
		Item *GetParent();
		void SetParent(Item *parent);

		bool operator ==(ValueType type) const;
		bool operator !=(ValueType type) const;

		bool IsOptional() const;
		void SetOptional(bool is_optional);

		bool IsIncludable() const;
		void SetIncludable(bool is_includable);

		bool IsOverridable() const;
		void SetOverridable(bool is_overridable);

		bool IsParsed() const;
		void SetParsed(bool is_parsed);

		virtual bool IsValid() const;
		virtual void Reset() = 0;
		virtual bool ParseFromValue(const ValueBase *value, int indent) = 0;
		virtual bool ParseFromAttribute(const pugi::xml_attribute &attribute, bool processing_include_file, int indent)
		{
			return false;
		}
		virtual bool ParseFromNode(const pugi::xml_node &node, bool processing_include_file, int indent) = 0;

		virtual ov::String ToString() const;
		virtual ov::String ToString(int indent, bool append_new_line) const;

		static ov::String MakeIndentString(int indent);

	protected:
		virtual ov::String ToStringInternal(int indent, bool append_new_line) const = 0;

		bool ProcessItem(Item *item, const pugi::xml_node &node, bool processing_include_file, int indent);

		ov::String _name;
		ValueType _type = ValueType::Unknown;

		Item *_parent = nullptr;

		bool _is_optional = false;
		bool _is_includable = false;
		bool _is_overridable = false;

		bool _is_parsed = false;
	};
}