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

class StunUnknownAttribute : public StunAttribute
{
public:
	StunUnknownAttribute(int type, int length);
	virtual ~StunUnknownAttribute();

	bool Parse(ov::ByteStream &stream) override;

	bool SetData(const void *data, int length);

	bool Serialize(ov::ByteStream &stream) const noexcept override;

	ov::String ToString() const override;

private:
	std::shared_ptr<ov::Data> _data;
};

