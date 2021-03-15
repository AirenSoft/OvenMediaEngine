#pragma once

#define OV_COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))
#define OV_PTR_FORMAT "0x%016" PRIxPTR

#if __cplusplus
#	define OV_GLOBAL_NAMESPACE_PREFIX ::
#else  // __cplusplus
#	define OV_GLOBAL_NAMESPACE_PREFIX
#endif	// __cplusplus

// If <x> is not equal to <value>, execute <exec>, and put <value> in <var>
#define OV_SAFE_RESET(x, value, exec, var) \
	do                                     \
	{                                      \
		if ((x) != (value))                \
		{                                  \
			exec;                          \
			(var) = (value);               \
		}                                  \
	} while (false)

// Example:
//
// OV_SAFE_FUNC(val, nullptr, close_func, &)
//   if(val != nullptr) {
//     close_func(& val);
//     val = nullptr
//   }
#define OV_SAFE_FUNC(x, value, func, prefix) OV_SAFE_RESET(x, value, func(prefix(x)), x)

#define OV_SAFE_DELETE(x) OV_SAFE_FUNC(x, nullptr, delete, )
#define OV_SAFE_FREE(x) OV_SAFE_FUNC(x, nullptr, OV_GLOBAL_NAMESPACE_PREFIX free, )	 // NOLINT
#define OV_CHECK_FLAG(x, flag) (((x) & (flag)) == (flag))

// value에서 index(0-based)번째 비트의 값을 얻어옴. 아래 식 대신 (((value) & (1 << index)) >> index) 를 사용해도 됨
// 예) ~는 계산해야 할 부분의 bit, !는 shift로 생긴 bit
// OV_GET_BIT(0b1010, 2) == 0b0
//               ~            ~
// value가 4bit 일 경우,
// (0b1010 >> 2) & 0b1 = 0b0010 & 0b1 = 0b0000 = 0b0
//     ~                   !! ~              ~     ~
#define OV_GET_BIT(value, index) (((value) >> (index)) & 1)

// value에서 index(0-based)번째 부터 (index + count)까지의 비트를 얻어옴
// 예) ~는 계산해야 할 부분의 bit, !는 shift로 생긴 bit
// OV_GET_BITS(0b00111100, 5, 2) == 0b01
//                ~~                  ~~
// value가 8bit 일 경우,
// 1) (index + count) bit를 left shift하여 MSB로 만듦
// (0b00111100 << (8 - (index + count)) = (0b00111100 << 1) = 0b01111000
//     ~~                                     ~~                ~~     !
// 2) 1번의 결과를 (index + (8 - (index + count))), 즉 (8 - count) 만큼 right shift하여 값 추출
// 0b01111000 >> (8 - count) = 0b01111000 >> 6 = 0b00000001 = 0b01
//   ~~     !                    ~~     ~          !!!!!!~~     ~~
#define OV_GET_BITS(type, value, index, count) ((type)((type)(((type)(value)) << ((sizeof(type) * 8) - ((index) + (count)))) >> ((sizeof(type) * 8) - (count))))