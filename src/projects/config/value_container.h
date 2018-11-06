//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "value_base.h"

namespace cfg
{
	template<typename Ttype>
	class ValueContainer : public ValueBase
	{
	public:
		ValueContainer(ValueType config_type) // NOLINT
		{
			SetType(config_type);
		}

		operator Ttype *() // NOLINT
		{
			return _value.get();
		}

		operator const Ttype *() const // NOLINT
		{
			return _value.get();
		}

		ov::String ToStringInternal(int indent, bool append_new_line) const override
		{
			return "";
		}

	protected:
		std::shared_ptr<Ttype> _value;
	};
}