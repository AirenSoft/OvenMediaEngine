//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stun_xor_address_attribute_format.h"
#include "modules/ice/ice_private.h"

StunXorAddressAttributeFormat::StunXorAddressAttributeFormat(StunAttributeType type, int length)
	: StunAddressAttributeFormat(type, length)
{

}

bool StunXorAddressAttributeFormat::Parse(ov::ByteStream &stream)
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
	if(StunAddressAttributeFormat::Parse(stream))
	{
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

	// Since the length is obtained in StunAddressAttribute::Parse(), 
	// there is no need to calculate _length

	return true;
}

bool StunXorAddressAttributeFormat::Serialize(ov::ByteStream &stream) const noexcept
{
	OV_ASSERT(_address.GetFamily() == ov::SocketFamily::Inet, "Only IPv4 is supprted");

	ov::SocketAddress address1 = _address;

	// xoring
	(static_cast<in_addr *>(address1))->s_addr ^= ov::HostToNetwork32(OV_STUN_MAGIC_COOKIE);
	address1.SetPort(static_cast<uint16_t>(address1.Port() ^ (OV_STUN_MAGIC_COOKIE >> 16)));

	return StunAttribute::Serialize(stream) &&
	       stream.Write8(0x00) &&
	       stream.Write8((uint8_t)GetFamily()) &&
	       stream.WriteBE16((uint16_t)address1.Port()) &&	
	       stream.Write(address1.ToAddrIn(), GetAddressLength());
}

ov::String StunXorAddressAttributeFormat::ToString() const
{
	return StunAddressAttributeFormat::ToString(StringFromType(GetType()));
}