//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <vector>
#include <string>
#include <memory>

#include <base/ovlibrary/ovlibrary.h>
#include <pugixml-1.9/src/pugixml.hpp>

namespace cfg
{
	//region ValueType

	enum class ValueType
	{
		// Unknown type
			Unknown = 'U',
		// A text of the child element
			String = 'S',
		// An integer value of the child element
			Integer = 'I',
		// A boolean value of the child element
			Boolean = 'B',
		// A float value of the child element
			Float = 'F',
		// A text of the element
			Text = 'T',
		// An attribute of the element
			Attribute = 'A',
		// An element of the child element
			Element = 'E',
		// An element list of the child element
			List = 'L',
	};

	//endregion

	//region ValueBase

	class ValueBase
	{
	public:
		virtual ~ValueBase() = default;

		ValueType GetType() const;
		void SetType(ValueType type);

		bool IsOptional() const;
		void SetOptional(bool is_optional);

		bool IsNeedToResolvePath() const;
		void SetNeedToResolvePath(bool need_to_resolve_path);

		size_t GetSize() const;

		void *GetTarget() const;

	protected:
		ValueType _type = ValueType::Unknown;

		bool _is_optional = false;
		bool _need_to_resolve_path = false;

		size_t _value_size = 0;
		void *_target = nullptr;
	};

	//endregion
}