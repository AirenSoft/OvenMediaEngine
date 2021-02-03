//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stun_attribute.h"

#include "modules/ice/ice_private.h"

// Attributes
#include "stun_mapped_address_attribute.h"
#include "stun_xor_mapped_address_attribute.h"
#include "stun_xor_relayed_address_attribute.h"
#include "stun_xor_peer_address_attribute.h"
#include "stun_user_name_attribute.h"
#include "stun_message_integrity_attribute.h"
#include "stun_fingerprint_attribute.h"
#include "stun_requested_transport_attribute.h"
#include "stun_unknown_attribute.h"

StunAttribute::StunAttribute(StunAttributeType type, uint16_t type_number, size_t length)
	: _type(type),
	  _type_number(type_number),
	  _length(length)
{
}

StunAttribute::StunAttribute(StunAttributeType type, size_t length)
	: StunAttribute(type, static_cast<uint16_t>(type), length)
{
}

StunAttribute::StunAttribute(const StunAttribute &type)
	: _type(type._type),
	  _type_number(type._type_number),
	  _length(type._length)
{
}

StunAttribute::StunAttribute(StunAttribute &&type) noexcept
	: _type(type._type),
	  _type_number(type._type_number),
	  _length(type._length)
{
	type._type = StunAttributeType::UnknownAttributes;
	type._type_number = 0;
	type._length = 0;
}

StunAttribute::~StunAttribute()
{
}

std::unique_ptr<StunAttribute> StunAttribute::CreateAttribute(ov::ByteStream &stream)
{
	if(stream.Remained() % 4 != 0)
	{
		logtw("Invalid padding: %d bytes", stream.Remained());
		return nullptr;
	}

	StunAttributeType type = static_cast<StunAttributeType>(stream.ReadBE16());
	uint16_t length = stream.ReadBE16();
	auto padded_length = static_cast<uint16_t>(((length % 4) > 0) ? ((length / 4) + 1) * 4 : length);

	if(stream.Remained() < padded_length)
	{
		logtw("Data is too short: type: 0x%04X, data length: %d (expected: %d)", type, stream.Remained(), padded_length);
		return nullptr;
	}

#if STUN_LOG_DATA
	logtd("Parsing attribute: type: 0x%04X, length: %d (padded: %d)...\n%s", type, length, padded_length, stream.Dump(padded_length).CStr());
#else // STUN_LOG_DATA
	logtd("Parsing attribute: type: 0x%04X, length: %d (padded: %d)...", type, length, padded_length);
#endif // STUN_LOG_DATA

#if DEBUG
	off_t last_offset = stream.GetOffset();
#endif // DEBUG

	if(type == StunAttributeType::Fingerprint)
	{
		// fingerprint must be last attribute
		OV_ASSERT2(stream.Remained() == length);

		if(stream.Remained() != length)
		{
			logtw("Fingerprint attribute MUST BE last attribute");
			return nullptr;
		}
	}

	std::unique_ptr<StunAttribute> attribute = CreateAttribute(type, length);

	if(attribute == nullptr)
	{
		// Unimplemented attributes
		logtd("Skipping attribute (not implemented): 0x%04X (%d bytes)...", type, length);
		stream.Skip<uint8_t>(length);
	}
	else
	{
		if(attribute->Parse(stream) == false)
		{
			logtw("Could not parse attribute: type: 0x%04X, length: %d", type, length);
			return nullptr;
		}

		logtd("Parsed: %s", attribute->ToString().CStr());

#if DEBUG
		OV_ASSERT(length == (stream.GetOffset() - last_offset), "Length is mismatch. (expected: %d, read length: %d)", length, (stream.GetOffset() - last_offset));
#endif // DEBUG

		stream.Skip<uint8_t>(padded_length - length);
	}

	return std::move(attribute);
}

std::unique_ptr<StunAttribute> StunAttribute::CreateAttribute(StunAttributeType type, int length)
{
	std::unique_ptr<StunAttribute> attribute = nullptr;

	switch(type)
	{
		case StunAttributeType::MappedAddress:
			attribute = std::make_unique<StunMappedAddressAttribute>(length);
			break;
		case StunAttributeType::XorPeerAddress:
			attribute = std::make_unique<StunXorPeerAddressAttribute>(length);
			break;
		case StunAttributeType::XorRelayedAddress:
			attribute = std::make_unique<StunXorRelayedAddressAttribute>(length);
			break;
		case StunAttributeType::XorMappedAddress:
			attribute = std::make_unique<StunXorMappedAddressAttribute>(length);
			break;
		case StunAttributeType::UserName:
			attribute = std::make_unique<StunUserNameAttribute>(length);
			break;
		case StunAttributeType::MessageIntegrity:
			attribute = std::make_unique<StunMessageIntegrityAttribute>(length);
			break;
		case StunAttributeType::Fingerprint:
			attribute = std::make_unique<StunFingerprintAttribute>(length);
			break;
		case StunAttributeType::RequestedTransport:
			attribute = std::make_unique<StunRequestedTransportAttribute>(length);
			break;
		case StunAttributeType::ErrorCode:
		case StunAttributeType::Realm:
		case StunAttributeType::Nonce:
		case StunAttributeType::Software:
		case StunAttributeType::AlternateServer:
			break;

		case StunAttributeType::UnknownAttributes:
		default:
			switch(static_cast<int>(type))
			{
				case 0x8029:
					// 0x8029 ICE-CONTROLLED
				case 0x802A:
					// 0x802A ICE-CONTROLLING
				case 0xC057:
					// 0xC057 NETWORK COST
				case 0x0025:
					// 0x0025 USE-CANDIDATE
				case 0x0024:
					// 0x0024 PRIORITY
					break;

				default:
					// 잘못된 타입이 들어옴
					logtw("Unknown attributes: %d (%x, length: %d)", type, type, length);
					break;
			}

			attribute = std::make_unique<StunUnknownAttribute>((int)type, length);
			break;
	}

	return std::move(attribute);
}

StunAttributeType StunAttribute::GetType() const noexcept
{
	return _type;
}

uint16_t StunAttribute::GetTypeNumber() const noexcept
{
	return _type_number;
}

size_t StunAttribute::GetLength(bool include_header, bool padding) const noexcept
{
	size_t length = _length + (include_header ? DefaultHeaderSize() : 0);

	if(padding)
	{
		if((length % 4) > 0)
		{
			return ((length / 4) + 1) * 4;
		}
	}

	return length;
}

bool StunAttribute::Serialize(ov::ByteStream &stream) const noexcept
{
	// Attribute header: Type + Length + (variable)
	// 여기서는 Type + Length만 기록하고, variable는 하위 클래스들에서 기록함
	return stream.WriteBE16(_type_number) && stream.WriteBE16(static_cast<uint16_t>(_length));
}

const char *StunAttribute::StringFromType(StunAttributeType type) noexcept
{
	switch(type)
	{
		case StunAttributeType::MappedAddress:
			return "MAPPED-ADDRESS";
		case StunAttributeType::XorMappedAddress:
			return "XOR-MAPPED-ADDRESS";
		case StunAttributeType::UserName:
			return "USERNAME";
		case StunAttributeType::MessageIntegrity:
			return "MESSAGE-INTEGRITY";
		case StunAttributeType::Fingerprint:
			return "FINGERPRINT";
		case StunAttributeType::ErrorCode:
			return "ERROR-CODE";
		case StunAttributeType::Realm:
			return "REALM";
		case StunAttributeType::Nonce:
			return "NONCE";
		case StunAttributeType::UnknownAttributes:
			return "UNKNOWN-ATTRIBUTES";
		case StunAttributeType::Software:
			return "SOFTWARE";
		case StunAttributeType::AlternateServer:
			return "ALTERNATE-SERVER";
		case StunAttributeType::ChannelNumber:
			return "CHANNEL-NUMBER";
		case StunAttributeType::Lifetime:
			return "LIFETIME";
		case StunAttributeType::XorPeerAddress:
			return "XOR-PEER-ADDRESS";
		case StunAttributeType::Data:
			return "DATA";
		case StunAttributeType::XorRelayedAddress:
			return "XOR-RELAYED-ADDRESS";
		case StunAttributeType::RequestedAddressFamily:
			return "REQUESTED-ADDRESS-FAMILY";
		case StunAttributeType::EvenPort:
			return "EVEN-PORT";
		case StunAttributeType::RequestedTransport:
			return "REQUESTED-TRANSPORT";
		case StunAttributeType::DontFragment:
			return "DONT-FRAGMENT";
		case StunAttributeType::ReservationToken:
			return "RESERVATION-TOKEN";
		case StunAttributeType::AdditionalAddressFamily:
			return "ADDITIONAL-ADDRESS-FAMILY";
		case StunAttributeType::AddressErrorCode:
			return "ADDRESS-ERROR-CODE";
		case StunAttributeType::ICMP:
			return "ICMP";
	}

	return "<UNKNOWN>";
}

const char *StunAttribute::StringFromErrorCode(StunErrorCode code) noexcept
{
	switch(code)
	{
		case StunErrorCode::TryAlternate:
			return "Try Alternate";
		case StunErrorCode::BadRequest:
			return "Bad Request";
		case StunErrorCode::Unauthonticated:
			return "Unauthenticated";
		case StunErrorCode::Forbidden:
			return "Forbidden";
		case StunErrorCode::UnknownAttribute:
			return "Unknown Attribute";
		case StunErrorCode::AllocationMismatch:
			return "Allocation Mismatch";
		case StunErrorCode::StaleNonce:
			return "Stale Nonce";
		case StunErrorCode::AddressFamilyNotSupported:
			return "Address Family Not Supported";
		case StunErrorCode::WrongCredentials:
			return "Wrong Credentials";
		case StunErrorCode::UnsupportedTransportProtocol:
			return "Unsupported Transport Protocol";
		case StunErrorCode::PeerAddressFamilyMismatch:
			return "Peer Address Family Mismatch";
		case StunErrorCode::AllocationQuotaReached:
			return "Allocation Quota Reached";
		case StunErrorCode::ServerError:
			return "Server Error";
		case StunErrorCode::InsufficientCapacity:
			return "Insufficient Capacity";
	}

	return "<UNKNOWN>";
}

const char *StunAttribute::StringFromType() const noexcept
{
	return StringFromType(_type);
}

ov::String StunAttribute::ToString() const
{
	return ToString("StunAttribute", "");
}

ov::String StunAttribute::ToString(const char *class_name, const char *suffix) const
{
	ov::String type;

	if(_type == StunAttributeType::UnknownAttributes)
	{
		type.Format("%s - %04X", StringFromType(), _type_number);
	}
	else
	{
		type.Format("%s", StringFromType());
	}

	return ov::String::FormatString("<%s: %s, length: %d%s>", class_name, type.CStr(), _length, suffix);
}