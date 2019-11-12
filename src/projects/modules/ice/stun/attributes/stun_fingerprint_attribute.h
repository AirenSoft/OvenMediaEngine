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

#include "modules/ice/stun/stun_datastructure.h"

class StunFingerprintAttribute : public StunAttribute
{
public:
	StunFingerprintAttribute();
	StunFingerprintAttribute(int length);
	virtual ~StunFingerprintAttribute();

	virtual bool Parse(ov::ByteStream &stream) override;

	uint32_t GetCrc() const;
	bool SetCrc(uint32_t crc);

	bool Serialize(ov::ByteStream &stream) const noexcept override;

	ov::String ToString() const override;

protected:
	uint32_t _crc;
};

