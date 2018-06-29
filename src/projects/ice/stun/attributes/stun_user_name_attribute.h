//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stun_attribute.h"

class StunUserNameAttribute : public StunAttribute
{
public:
	StunUserNameAttribute();
	StunUserNameAttribute(int length);
	virtual ~StunUserNameAttribute();

	bool Parse(ov::ByteStream &stream) override;

	const ov::String &GetUserName() const
	{
		return _user_name;
	}

	bool SetUserName(const ov::String &name);

	bool Serialize(ov::ByteStream &stream) const noexcept override;

	ov::String ToString() const override;

protected:
	ov::String _user_name;
};
