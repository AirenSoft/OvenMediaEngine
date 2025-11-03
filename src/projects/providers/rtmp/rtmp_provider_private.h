//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define OV_LOG_TAG "RTMPProvider"

#define RTMP_STREAM_FORMAT_PREFIX \
	"[%s/%s(%u)] "

#define RTMP_STREAM_DESC \
	_vhost_app_name.CStr(), GetName().CStr(), GetId()

#define logat(format, ...) logtt(RTMP_STREAM_FORMAT_PREFIX format, RTMP_STREAM_DESC, ##__VA_ARGS__)
#define logad(format, ...) logtd(RTMP_STREAM_FORMAT_PREFIX format, RTMP_STREAM_DESC, ##__VA_ARGS__)
#define logai(format, ...) logti(RTMP_STREAM_FORMAT_PREFIX format, RTMP_STREAM_DESC, ##__VA_ARGS__)
#define logaw(format, ...) logtw(RTMP_STREAM_FORMAT_PREFIX format, RTMP_STREAM_DESC, ##__VA_ARGS__)
#define logae(format, ...) logte(RTMP_STREAM_FORMAT_PREFIX format, RTMP_STREAM_DESC, ##__VA_ARGS__)
#define logac(format, ...) logtc(RTMP_STREAM_FORMAT_PREFIX format, RTMP_STREAM_DESC, ##__VA_ARGS__)
