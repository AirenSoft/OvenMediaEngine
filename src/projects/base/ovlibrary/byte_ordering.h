//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <cinttypes>
#include <endian.h>

#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#   define OV_IS_LITTLE_ENDIAN                                      1
#elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#   define OV_IS_LITTLE_ENDIAN                                      0
#else
#   error Could not identify byte ordering
#endif

// OV_SELECT_BY_ENDIAN(call_when_little_endian, call_when_big_endian) 정의
#if OV_IS_LITTLE_ENDIAN
#   define OV_SELECT_BY_ENDIAN(call_when_little_endian, call_when_big_endian) \
    call_when_little_endian
#else // OV_IS_LITTLE_ENDIAN
#   define OV_SELECT_BY_ENDIAN(call_when_little_endian, call_when_big_endian) \
	call_when_big_endian
#endif // OV_IS_LITTLE_ENDIAN

#define OV_DECLARE_ENDIAN_FUNCTION(type, name, func) \
    inline type name(type value) \
    { \
        return func(value); \
    }

// 기존에 endian.h 에 define 되어 있는 값들은 타입 강제성이 없어서 사용하기 불편한 부분이 있음
// 이를 해결하기 위해 재정의
namespace ov
{
	inline bool IsLittleEndian() noexcept
	{
		return OV_SELECT_BY_ENDIAN(false, true);
	}

	inline bool IsBigEndian() noexcept
	{
		return OV_SELECT_BY_ENDIAN(false, true);
	}

	// endian 변경 예)
	// uint16_t value = HostToLE16(0x00F0);
	// uint32_t value = HostToLE32(0x00FF00F0);

	// Host => Little Endian
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, HostToLE16, htole16);

	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, HostToLE32, htole32);

	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, HostToLE64, htole64);

	// Little Endian => Host
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, LE16ToHost, le16toh);

	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, LE32ToHost, le32toh);

	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, LE64ToHost, le64toh);

	// Host => Big Endian
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, HostToBE16, htobe16);

	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, HostToBE32, htobe32);

	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, HostToBE64, htobe64);

	// Big Endian => Host
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, BE16ToHost, be16toh);

	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, BE32ToHost, be32toh);

	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, BE64ToHost, be64toh);

	// Host => Network Endian (Host => BE의 alias)
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, HostToNetwork16, htobe16);

	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, HostToNetwork32, htobe32);

	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, HostToNetwork64, htobe64);

	// Network Endian => Host (BE => Host의 alias)
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, NetworkToHost16, be16toh);

	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, NetworkToHost32, be32toh);

	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, NetworkToHost64, be64toh);

}
