//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#if defined(__APPLE__)
#include <machine/endian.h>
#include <libkern/OSByteOrder.h>

#define htole16(x) OSSwapHostToLittleInt16(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define le16toh(x) OSSwapLittleToHostConstInt16(x)
#define le32toh(x) OSSwapLittleToHostConstInt32(x)
#define le64toh(x) OSSwapLittleToHostConstInt64(x)
#define htobe16(x) OSSwapHostToBigInt16(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be16toh(x) OSSwapBigToHostConstInt16(x)
#define be32toh(x) OSSwapBigToHostConstInt32(x)
#define be64toh(x) OSSwapBigToHostConstInt64(x)
#else
#include <endian.h>
#include <arpa/inet.h>
#endif
#include <cinttypes>

#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#	define OV_IS_LITTLE_ENDIAN 1
#elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#	define OV_IS_LITTLE_ENDIAN 0
#else
#	error Could not identify byte ordering
#endif

#pragma pack(push, 1)
struct uint24_t
{
	uint24_t(uint32_t value) noexcept
		: data(value)
	{
	}

	uint24_t()
	{
		data = 0U;
	}

	uint24_t(const uint24_t &value) noexcept = default;
	uint24_t(uint24_t &&value) noexcept = default;

	unsigned int data : 24;

	inline operator uint32_t() const noexcept
	{
		return data;
	}

	uint24_t &operator=(const uint24_t &value) noexcept
	{
		data = value.data;
		return *this;
	}
};

struct int24_t
{
	int24_t(int32_t value) noexcept
		: data(value)
	{
	}

	int24_t()
	{
		data = 0;
	}

	int24_t(const int24_t &value) noexcept = default;
	int24_t(int24_t &&value) noexcept = default;

	int data : 24;

	inline operator int32_t() const noexcept
	{
		// sign extension
		return (data << 8) >> 8;
	}

	int24_t &operator=(const int24_t &value) noexcept
	{
		data = value.data;
		return *this;
	}
};

#pragma pack(pop)

// OV_SELECT_BY_ENDIAN(call_when_little_endian, call_when_big_endian) 정의
#if OV_IS_LITTLE_ENDIAN
#	define OV_SELECT_BY_ENDIAN(call_when_little_endian, call_when_big_endian) \
		call_when_little_endian
#else  // OV_IS_LITTLE_ENDIAN
#	define OV_SELECT_BY_ENDIAN(call_when_little_endian, call_when_big_endian) \
		call_when_big_endian
#endif  // OV_IS_LITTLE_ENDIAN

#define OV_DECLARE_ENDIAN_FUNCTION(type, name, func) \
	inline type name(type value)                     \
	{                                                \
		return func(value);                          \
	}

inline uint24_t be24toh(uint24_t value) noexcept
{
	return OV_SELECT_BY_ENDIAN(be32toh(value << 8), value);
}

inline uint24_t htobe24(uint24_t value) noexcept
{
	return OV_SELECT_BY_ENDIAN(htobe32(value) >> 8, value);
}

inline uint24_t le24toh(uint24_t value) noexcept
{
	return OV_SELECT_BY_ENDIAN(value, le32toh(value << 8));
}

inline uint24_t htole24(uint24_t value) noexcept
{
	return OV_SELECT_BY_ENDIAN(value, htole32(value) >> 8);
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
	OV_DECLARE_ENDIAN_FUNCTION(uint24_t, HostToLE24, htole24);
	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, HostToLE32, htole32);
	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, HostToLE64, htole64);

	// Little Endian => Host
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, LE16ToHost, le16toh);
	OV_DECLARE_ENDIAN_FUNCTION(uint24_t, LE24ToHost, le24toh);
	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, LE32ToHost, le32toh);
	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, LE64ToHost, le64toh);

	// Host => Big Endian
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, HostToBE16, htobe16);
	OV_DECLARE_ENDIAN_FUNCTION(uint24_t, HostToBE24, htobe24);
	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, HostToBE32, htobe32);
	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, HostToBE64, htobe64);

	// Big Endian => Host
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, BE16ToHost, be16toh);
	OV_DECLARE_ENDIAN_FUNCTION(uint24_t, BE24ToHost, be24toh);
	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, BE32ToHost, be32toh);
	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, BE64ToHost, be64toh);

	// Host => Network Endian (alias of Host => BE)
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, HostToNetwork16, HostToBE16);
	OV_DECLARE_ENDIAN_FUNCTION(uint24_t, HostToNetwork24, HostToBE24);
	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, HostToNetwork32, HostToBE32);
	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, HostToNetwork64, HostToBE64);

	// Network Endian => Host (alias of BE => Host)
	OV_DECLARE_ENDIAN_FUNCTION(uint16_t, NetworkToHost16, BE16ToHost);
	OV_DECLARE_ENDIAN_FUNCTION(uint24_t, NetworkToHost24, BE24ToHost);
	OV_DECLARE_ENDIAN_FUNCTION(uint32_t, NetworkToHost32, BE32ToHost);
	OV_DECLARE_ENDIAN_FUNCTION(uint64_t, NetworkToHost64, BE64ToHost);

}  // namespace ov
