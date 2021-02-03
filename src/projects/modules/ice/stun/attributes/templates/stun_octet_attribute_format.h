//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "modules/ice/stun/attributes/stun_attributes.h"
#include "modules/ice/stun/stun_datastructure.h"

template<typename T>
class StunOctetAttributeFormat : public StunAttribute
{
public:
	virtual bool Parse(ov::ByteStream &stream) override
	{
		if(stream.IsRemained(sizeof(T)) == false)
		{
			return false;
		}

		stream.ReadBE(static_cast<T&>(_value));

		return true;
	}

	uint32_t GetValue() const
	{
		return _value;
	}

	bool SetValue(T value)
	{
		_value = value;
		return true;
	}

	bool Serialize(ov::ByteStream &stream) const noexcept override
	{
		return StunAttribute::Serialize(stream) && stream.WriteBE(static_cast<T>(_value));
	}

	ov::String ToString() const override
	{
		return StunAttribute::ToString(StringFromType(GetType()), ov::String::FormatString(", value : %08X", _value).CStr());
	}

protected:
	StunOctetAttributeFormat(StunAttributeType type, int length) : StunAttribute(type, length) {}

	T _value;
};

