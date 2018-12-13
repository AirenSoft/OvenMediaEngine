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
#define MAX_BUFFER_SIZE                      4096

#define ADD_FLAG_IF(list, x, flag) \
    if(OV_CHECK_FLAG(x, flag)) \
    { \
        list.emplace_back(# flag); \
    }

// state가 condition이 아니면 return_value를 반환함
#define CHECK_STATE(condition, return_value) \
    do \
    { \
        if(!(_state condition)) \
        { \
            logte("[#%d] Invalid state: %d (expected: _state(%d) " # condition ")", _socket, _state, _state); \
            return return_value; \
        } \
    } while(false)

#define CHECK_STATE2(condition1, condition2, return_value) \
    do \
    { \
        if(!(_state condition1)) \
        { \
            logte("[#%d] Invalid state: %d (expected: _state(%d) " # condition1 ")", _socket, _state, _state); \
            return return_value; \
        } \
        if(!(_state condition2)) \
        { \
            logte("[#%d] Invalid state: %d (expected: _state(%d) " # condition2 ")", _socket, _state, _state); \
            return return_value; \
        } \
    } while(false)
