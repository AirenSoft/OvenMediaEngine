//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "value_base.h"

#include "item.h"

namespace cfg
{
	ValueType ValueBase::GetType() const
	{
		return _type;
	}

	void ValueBase::SetType(ValueType type)
	{
		_type = type;
	}

	bool ValueBase::IsOptional() const
	{
		return _is_optional || ((_optional_callback == nullptr) ? false : _optional_callback());
	}

	void ValueBase::SetOptional(bool is_optional)
	{
		_is_optional = is_optional;
	}

	void ValueBase::SetOptionalCallback(OptionalCallback optional_callback)
	{
		_optional_callback = optional_callback;
	}

	void ValueBase::SetValidationCallback(ValidationCallback validation_callback)
	{
		_validation_callback = validation_callback;
	}

	bool ValueBase::DoValidation()
	{
		return (_validation_callback == nullptr) ? true : _validation_callback();
	}

	bool ValueBase::IsNeedToResolvePath() const
	{
		return _need_to_resolve_path;
	}

	void ValueBase::SetNeedToResolvePath(bool need_to_resolve_path)
	{
		_need_to_resolve_path = need_to_resolve_path;
	}

	size_t ValueBase::GetSize() const
	{
		return _value_size;
	}

	void *ValueBase::GetTarget() const
	{
		return _target;
	}
}  // namespace cfg