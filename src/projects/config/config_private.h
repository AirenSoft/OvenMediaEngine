//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "utilities/config_utility.h"

#define OV_LOG_TAG "Config"

#define logap(format, ...) logtp("[%p] " format, this, ##__VA_ARGS__)
#define logad(format, ...) logtd("[%p] " format, this, ##__VA_ARGS__)
#define logas(format, ...) logts("[%p] " format, this, ##__VA_ARGS__)

#define logai(format, ...) logti("[%p] " format, this, ##__VA_ARGS__)
#define logaw(format, ...) logtw("[%p] " format, this, ##__VA_ARGS__)
#define logae(format, ...) logte("[%p] " format, this, ##__VA_ARGS__)
#define logac(format, ...) logtc("[%p] " format, this, ##__VA_ARGS__)

#define HANDLE_CAST_EXCEPTION(value_type, prefix, ...) \
	logad(                                             \
		prefix                                         \
		"Could not convert value:\n"                   \
		"\tType: %s\n"                                 \
		"\tFrom: %s\n"                                 \
		"\t  To: %s",                                  \
		##__VA_ARGS__,                                 \
		StringFromValueType(value_type),               \
		cast_exception.from.CStr(),                    \
		cast_exception.to.CStr());                     \
                                                       \
	throw CreateConfigError(                           \
		prefix                                         \
		"Could not convert value - from: %s, to: %s",  \
		##__VA_ARGS__,                                 \
		cast_exception.from.CStr(),                    \
		cast_exception.to.CStr())
