//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

//--------------------------------------------------------------------
// Related to FFmpeg
//--------------------------------------------------------------------
const char *GetFFmpegConfiguration();
const char *GetFFmpegVersion();
const char *GetFFmpegAvFormatVersion();
const char *GetFFmpegAvCodecVersion();
const char *GetFFmpegAvUtilVersion();
const char *GetFFmpegAvFilterVersion();
const char *GetFFmpegSwResampleVersion();
const char *GetFFmpegSwScaleVersion();
std::shared_ptr<ov::Error> InitializeFFmpeg();
std::shared_ptr<ov::Error> TerminateFFmpeg();

//--------------------------------------------------------------------
// Related to SRTP
//--------------------------------------------------------------------
const char *GetSrtpVersion();
std::shared_ptr<ov::Error> InitializeSrtp();
std::shared_ptr<ov::Error> TerminateSrtp();

//--------------------------------------------------------------------
// Related to SRT
//--------------------------------------------------------------------
const char *GetSrtVersion();
std::shared_ptr<ov::Error> InitializeSrt();
std::shared_ptr<ov::Error> TerminateSrt();

//--------------------------------------------------------------------
// Related to OpenSSL
//--------------------------------------------------------------------
const char *GetOpenSslConfiguration();
const char *GetOpenSslVersion();
std::shared_ptr<ov::Error> InitializeOpenSsl();
std::shared_ptr<ov::Error> TerminateOpenSsl();

//--------------------------------------------------------------------
// Related to JsonCpp
//--------------------------------------------------------------------
const char *GetJsonCppVersion();

//--------------------------------------------------------------------
// Related to jemalloc
//--------------------------------------------------------------------
const char *GetJemallocVersion();

//--------------------------------------------------------------------
// Related to spdlog
//--------------------------------------------------------------------
const char *GetSpdlogVersion();