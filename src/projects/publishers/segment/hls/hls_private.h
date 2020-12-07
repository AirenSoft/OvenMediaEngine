//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define OV_LOG_TAG						"HLS"

#define HLS_SEGMENT_EXT 				"ts"
#define HLS_PLAYLIST_EXT 				"m3u8"
#define HLS_PLAYLIST_FILE_NAME 			"playlist.m3u8"

#define logad(format, ...)				logtd("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
#define logas(format, ...)				logts("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)

#define logai(format, ...)				logti("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
#define logaw(format, ...)				logtw("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
#define logae(format, ...)				logte("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
#define logac(format, ...)				logtc("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)