//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stun_user_name_attribute.h"

StunUserNameAttribute::StunUserNameAttribute()
	: StunUserNameAttribute(0)
{
}

StunUserNameAttribute::StunUserNameAttribute(int length)
	: StunAttribute(StunAttributeType::UserName, length)
{
	// RFC5389 - 15.3. USERNAME
	// The value of USERNAME is a variable-length value.  It MUST contain a
	// UTF-8 [RFC3629] encoded sequence of less than 513 bytes, and MUST
	// have been processed using SASLprep [RFC4013].

	OV_ASSERT2(length < 513);
}

StunUserNameAttribute::~StunUserNameAttribute()
{
}

bool StunUserNameAttribute::Parse(ov::ByteStream &stream)
{
	// 문자열이 복사될 공간 확보
	_user_name.SetLength(_length);

	return stream.Read<uint8_t>((uint8_t *)(_user_name.GetBuffer()), _length) == _length;
}

bool StunUserNameAttribute::SetUserName(const ov::String &name)
{
	_user_name = name;
	_length = name.GetLength();

	return true;
}

bool StunUserNameAttribute::Serialize(ov::ByteStream &stream) const noexcept
{
	return
		StunAttribute::Serialize(stream) &&
		stream.Write<uint8_t>((uint8_t *)(_user_name.CStr()), _user_name.GetLength());
}

ov::String StunUserNameAttribute::ToString() const
{
	return StunAttribute::ToString("StunUserNameAttribute", ov::String::FormatString(", name: %s", _user_name.CStr()).CStr());
}
