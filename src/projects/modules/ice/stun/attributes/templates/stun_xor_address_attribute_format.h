//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stun_address_attribute_format.h"

class StunXorAddressAttributeFormat : public StunAddressAttributeFormat
{
public:
	bool Parse(ov::ByteStream &stream) override;
	bool Serialize(ov::ByteStream &stream) const noexcept override;
	ov::String ToString() const override;

protected:
	StunXorAddressAttributeFormat(StunAttributeType type, int length);
};