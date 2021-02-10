//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define OV_LOG_TAG "LLDASH"

#define logad(format, ...)				logtd("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
#define logap(format, ...)				logtp("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
#define logas(format, ...)				logts("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)

#define logai(format, ...)				logti("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
#define logaw(format, ...)				logtw("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
#define logae(format, ...)				logte("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
#define logac(format, ...)				logtc("[%s/%s] %s: " format, _app_name.CStr(), _stream_name.CStr(), GetPacketizerName(), ##__VA_ARGS__)
