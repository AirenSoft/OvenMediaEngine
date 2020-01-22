//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stun_fingerprint_attribute.h"

#include "modules/ice/stun/stun_message.h"

StunFingerprintAttribute::StunFingerprintAttribute()
	: StunFingerprintAttribute(sizeof(_crc))
{
}

StunFingerprintAttribute::StunFingerprintAttribute(int length)
	: StunAttribute(StunAttributeType::Fingerprint, length),

	  _crc(0)
{
	// fingerprint attribute의 크기는 기본 크기 + 32bit
	OV_ASSERT2(DefaultHeaderSize() + sizeof(uint32_t));
}

StunFingerprintAttribute::~StunFingerprintAttribute()
{
}

bool StunFingerprintAttribute::Parse(ov::ByteStream &stream)
{
	// CRC (4B)
	if(stream.IsRemained(sizeof(uint32_t)) == false)
	{
		return false;
	}

	_crc = stream.ReadBE32();

	return true;
}

uint32_t StunFingerprintAttribute::GetCrc() const
{
	return _crc;
}

bool StunFingerprintAttribute::SetCrc(uint32_t crc)
{
	_crc = crc;

	return true;
}

bool StunFingerprintAttribute::Serialize(ov::ByteStream &stream) const noexcept
{
	return StunAttribute::Serialize(stream) &&
	       stream.WriteBE32(_crc);
}

ov::String StunFingerprintAttribute::ToString() const
{
	return StunAttribute::ToString("StunFingerprintAttribute", ov::String::FormatString(", crc: %08X", _crc).CStr());
}