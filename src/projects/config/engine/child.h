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

#include <functional>

#include "config_error.h"
#include "item_name.h"
#include "value_type.h"

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
}  // namespace cfg
