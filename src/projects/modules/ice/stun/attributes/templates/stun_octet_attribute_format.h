//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "modules/ice/stun/attributes/stun_attribute.h"
#include "modules/ice/stun/stun_datastructure.h"

template<typename T>
class StunOctetAttributeFormat : public StunAttribute
{
public:
	virtual bool Parse(const StunMessage *stun_message, ov::ByteStream &stream) override
	{
		if(stream.IsRemained(sizeof(T)) == false)
		{
			return false;
		}

		stream.ReadBE(_value);

		return true;
	}

	virtual T GetValue() const
	{
		return _value;
	}

	virtual bool SetValue(T value)
	{
		_value = value;
		return true;
	}

	bool Serialize(const StunMessage *stun_message, ov::ByteStream &stream) const noexcept override
	{
		return StunAttribute::Serialize(stun_message, stream) && stream.WriteBE(static_cast<T>(_value));
	}

	ov::String ToString() const override
	{
		return StunAttribute::ToString(StringFromType(GetType()), ov::String::FormatString(", value : %08X", _value).CStr());
	}

protected:
	StunOctetAttributeFormat(StunAttributeType type, int length) : StunAttribute(type, length) {}

	T _value;
};