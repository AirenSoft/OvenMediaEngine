//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define OV_LOG_TAG "Socket"

// UDP 까지 고려해서 적당히 크게 잡음
#define MAX_BUFFER_SIZE 4096

// state가 condition이 아니면 return_value를 반환함
#define CHECK_STATE(condition, return_value)                                 \
	do                                                                       \
	{                                                                        \
		if (!(_state condition))                                             \
		{                                                                    \
			logaw("%s(): Invalid state: %s (expected: " #condition ") - %s", \
				  __FUNCTION__,                                              \
				  StringFromSocketState(_state),                             \
				  ToString().CStr());                                        \
			return return_value;                                             \
		}                                                                    \
	} while (false)
