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
		return _is_optional;
	}

	void ValueBase::SetOptional(bool is_optional)
	{
		_is_optional = is_optional;
	}

	size_t ValueBase::GetSize() const
	{
		return _value_size;
	}

	void *ValueBase::GetTarget() const
	{
		return _target;
	}
}