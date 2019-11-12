//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stun_mapped_address_attribute.h"

class StunXorMappedAddressAttribute : public StunMappedAddressAttribute
{
public:
	StunXorMappedAddressAttribute();
	StunXorMappedAddressAttribute(int length);
	virtual ~StunXorMappedAddressAttribute();

	bool Parse(ov::ByteStream &stream) override;

	bool Serialize(ov::ByteStream &stream) const noexcept override;

	ov::String ToString() const override;

protected:
};

