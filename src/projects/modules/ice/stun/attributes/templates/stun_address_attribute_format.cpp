//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <arpa/inet.h>
#include "stun_address_attribute_format.h"
#include "modules/ice/ice_private.h"

StunAddressAttributeFormat::StunAddressAttributeFormat(StunAttributeType type, int length)
	: StunAttribute(type, length)
{
}

bool StunAddressAttributeFormat::Parse(ov::ByteStream &stream)
{
	//  0                   1                   2                   3
	//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// |0 0 0 0 0 0 0 0|    Family     |           Port                |
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	// |                                                               |
	// |                 Address (32 bits or 128 bits)                 |
	// |                                                               |
	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//
	//          Figure 5: Format of MAPPED-ADDRESS Attribute

	// Padding (1B) + Family (1B) + Port (2B)
	if(stream.IsRemained(sizeof(uint8_t) + sizeof(StunAddressFamily) + sizeof(uint16_t)) == false)
	{
		return false;
	}

	// Reserved
	stream.Skip<uint8_t>();

	// Family
	StunAddressFamily family = (StunAddressFamily)stream.Read8();

	if((family != StunAddressFamily::IPv4) && (family != StunAddressFamily::IPv6))
	{
		logtw("Invalid family: %d", family);
		return false;
	}

	// Port
	uint16_t port = stream.ReadBE16();
	
	// Address
	size_t address_length = (family == StunAddressFamily::IPv4) ? 4 : 16;

	if(stream.IsRemained(address_length) == false)
	{
		logtw("Could not read address. (Family: %d, Data is not enough: %d)", family, stream.Remained());
		return false;
	}

	switch(family)
	{
		case StunAddressFamily::IPv4:
		{
			_address.SetFamily(ov::SocketFamily::Inet);
			if(stream.Read<in_addr>(_address) != 1)
			{
				logtw("Could not read IPv4 address");
				return false;
			}

			_address.SetPort(port);
			_address.UpdateIPAddress();

			break;
		}

		case StunAddressFamily::IPv6:
			_address.SetFamily(ov::SocketFamily::Inet6);

			_address.SetPort(port);
			OV_ASSERT(false, "IPv6 is not supported");
			logte("IPv6 is not supported");
			return false;

		default:
			// 위에서 한 번 걸렀으므로, 여기로 진입하면 절대 안됨
			OV_ASSERT2(false);
			logtw("Invalid family: %d", family);
			return false;
	}

	return true;
}

StunAddressFamily StunAddressAttributeFormat::GetFamily() const
{
	switch(_address.GetFamily())
	{
		case ov::SocketFamily::Inet:
			return StunAddressFamily::IPv4;

		case ov::SocketFamily::Inet6:
			return StunAddressFamily::IPv6;

		default:
			return StunAddressFamily::Unknown;
	}
}

uint16_t StunAddressAttributeFormat::GetPort() const
{
	return _address.Port();
}

const ov::SocketAddress &StunAddressAttributeFormat::GetAddress() const
{
	return _address;
}

int StunAddressAttributeFormat::GetAddressLength() const
{
	return ((_address.GetFamily() == ov::SocketFamily::Inet) ? 4 : 16);
}

bool StunAddressAttributeFormat::SetParameters(const ov::SocketAddress &address)
{
	_address = address;

	// Padding (1B) + Family (1B) + Port (2B) + Address (4B or 16B)
	_length = sizeof(uint8_t) + sizeof(StunAddressFamily) + sizeof(uint16_t) + GetAddressLength();

	return true;
}

bool StunAddressAttributeFormat::Serialize(ov::ByteStream &stream) const noexcept
{
	return StunAttribute::Serialize(stream) &&
	       stream.Write8(0x00) &&
	       stream.Write8((uint8_t)GetFamily()) &&
	       stream.WriteBE16((uint16_t)GetPort()) &&
	       stream.Write(_address.AddrInForIPv4(), GetAddressLength());
}

ov::String StunAddressAttributeFormat::ToString(const char *class_name) const
{
	return StunAttribute::ToString(class_name, ov::String::FormatString(", address: %s:%d", _address.GetIpAddress().CStr(), GetPort()).CStr());
}

ov::String StunAddressAttributeFormat::ToString() const
{
	return ToString(StringFromType(GetType()));
}