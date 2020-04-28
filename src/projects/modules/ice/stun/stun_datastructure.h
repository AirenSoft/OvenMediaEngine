//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <netinet/in.h>

enum class StunClass : uint8_t
{
	Request = 0b00,
	Indication = 0b01,
	SuccessResponse = 0b10,
	ErrorResponse = 0b11,
};

enum class StunMethod : uint16_t
{
	Binding = 0x0001,
};

enum class StunAttributeType : uint16_t
{
	// 15.1. MAPPED-ADDRESS
	MappedAddress = 0x0001,
	// 15.2. XOR-MAPPED-ADDRESS
	XorMappedAddress = 0x0020,
	// 15.3. USERNAME
	UserName = 0x0006,
	// 15.4. MESSAGE-INTEGRITY
	MessageIntegrity = 0x0008,
	// 15.5. FINGERPRINT
	Fingerprint = 0x8028,
	// 15.6. ERROR-CODE
	ErrorCode = 0x0009,
	// 15.7. REALM
	Realm = 0x0014,
	// 15.8. NONCE
	Nonce = 0x0015,
	// 15.9. UNKNOWN-ATTRIBUTES
	//     0x8029 ICE-CONTROLLED
	//     0x802A ICE-CONTROLLING
	//     0xC057 NETWORK COST
	//     0x0025 USE-CANDIDATE
	//     0x0024 PRIORITY
	UnknownAttributes = 0x000A,
	// 15.10. SOFTWARE
	Software = 0x8022,
	// 15.11. ALTERNATE-SERVER
	AlternateServer = 0x8023,
};

// RFC5389, 15.2. XOR-MAPPED-ADDRESS
enum class StunAddressFamily : uint8_t
{
	Unknown = 0x00,

	IPv4 = 0x01,
	IPv6 = 0x02
};

// RFC5389, 15.6. ERROR-CODE
enum class StunErrorCode
{
	TryAlternate = 300,
	BadRequest = 400,
	Unauthorized = 401,
	UnknownAttribute = 420,
	StaleNonce = 438,
	ServerError = 500
};

// TODO: legacy 버전(RFC3489)에서는 길이가 다름. RFC3489는 나중에 추가할 것
#define OV_STUN_TRANSACTION_ID_LENGTH                           12

#define OV_STUN_FINGERPRINT_XOR_VALUE                           0x5354554E
#define OV_STUN_MAGIC_COOKIE                                    0x2112A442
// SHA1 digest 크기와 동일
#define OV_STUN_HASH_LENGTH                                     20
