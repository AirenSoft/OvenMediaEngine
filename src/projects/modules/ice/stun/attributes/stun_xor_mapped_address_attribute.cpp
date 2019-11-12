//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stun_xor_mapped_address_attribute.h"

#include "modules/ice/ice_private.h"

StunXorMappedAddressAttribute::StunXorMappedAddressAttribute()
	: StunXorMappedAddressAttribute(0)
{
}

StunXorMappedAddressAttribute::StunXorMappedAddressAttribute(int length)
	: StunMappedAddressAttribute(StunAttributeType::XorMappedAddress, length)
{
}

StunXorMappedAddressAttribute::~StunXorMappedAddressAttribute()
{
}

bool StunXorMappedAddressAttribute::Parse(ov::ByteStream &stream)
{
	//  0                   1                   2                   3
	//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// |x x x x x x x x|    Family     |         X-Port                |
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// |                X-Address (Variable)
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//
	//         Figure 6: Format of XOR-MAPPED-ADDRESS Attribute
	if(StunMappedAddressAttribute::Parse(stream))
	{
		// RFC 내용:
		// X-Port is computed by taking the mapped port in host byte order, XOR'ing it with the most significant 16 bits of the magic cookie, and then the converting the result to network byte order.
		int port = _address.Port();
		port = port ^ (OV_STUN_MAGIC_COOKIE >> 16);
		_address.SetPort(static_cast<uint16_t>(port));

		switch(_address.GetFamily())
		{
			case ov::SocketFamily::Inet:
				(static_cast<in_addr *>(_address))->s_addr ^= ov::HostToNetwork32(OV_STUN_MAGIC_COOKIE);
				_address.UpdateIPAddress();
				break;

			case ov::SocketFamily::Inet6:
				OV_ASSERT(false, "IPV6 IS NOT IMPLEMENTED");
				logte("IPv6 is not implemented");
				_length = 0;
				return false;

			default:
				logtw("Unknown family: %d", _address.GetFamily());
				_length = 0;
				return false;
		}

	}

	// StunMappedAddressAttribute::Parse() 안에서 길이가 구해졌으므로, 별도로 _length를 계산하지는 않음

	return true;
}

bool StunXorMappedAddressAttribute::Serialize(ov::ByteStream &stream) const noexcept
{
	// xoring을 위한 임시 변수
	OV_ASSERT(_address.GetFamily() == ov::SocketFamily::Inet, "Only IPv4 is supprted");

	ov::SocketAddress address1 = _address;

	// xoring
	(static_cast<in_addr *>(address1))->s_addr ^= ov::HostToNetwork32(OV_STUN_MAGIC_COOKIE);
	address1.SetPort(static_cast<uint16_t>(address1.Port() ^ (OV_STUN_MAGIC_COOKIE >> 16)));

	return StunAttribute::Serialize(stream) &&
	       stream.Write8(0x00) &&
	       stream.Write8((uint8_t)GetFamily()) &&
	       stream.WriteBE16((uint16_t)GetPort()) &&
	       stream.Write(address1.ToAddrIn(), GetAddressLength());
}

ov::String StunXorMappedAddressAttribute::ToString() const
{
	return StunMappedAddressAttribute::ToString("StunXorMappedAddressAttribute");
}