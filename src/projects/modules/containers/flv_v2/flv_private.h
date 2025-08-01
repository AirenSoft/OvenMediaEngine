//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define OV_LOG_TAG "Flv"

#define FLV_FORMAT_PREFIX \
	"[0x%08X] "

#define FLV_DESC \
	this

#define logap(format, ...) logtp(FLV_FORMAT_PREFIX format, FLV_DESC, ##__VA_ARGS__)
#define logad(format, ...) logtd(FLV_FORMAT_PREFIX format, FLV_DESC, ##__VA_ARGS__)
#define logas(format, ...) logts(FLV_FORMAT_PREFIX format, FLV_DESC, ##__VA_ARGS__)

#define logai(format, ...) logti(FLV_FORMAT_PREFIX format, FLV_DESC, ##__VA_ARGS__)
#define logaw(format, ...) logtw(FLV_FORMAT_PREFIX format, FLV_DESC, ##__VA_ARGS__)
#define logae(format, ...) logte(FLV_FORMAT_PREFIX format, FLV_DESC, ##__VA_ARGS__)
#define logac(format, ...) logtc(FLV_FORMAT_PREFIX format, FLV_DESC, ##__VA_ARGS__)
