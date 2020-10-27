//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "banner.h"

#include <sys/utsname.h>

#include "main.h"
#include "main_private.h"
#include "third_parties.h"

void PrintBanner()
{
	utsname uts{};
	::uname(&uts);

#if DEBUG
	static constexpr const char *BUILD_MODE = " [debug]";
#else	// DEBUG
	static constexpr const char *BUILD_MODE = "";
#endif	// DEBUG

	logti("OvenMediaEngine v" OME_VERSION OME_GIT_VERSION_EXTRA "%s is started on [%s] (%s %s - %s, %s)", BUILD_MODE, uts.nodename, uts.sysname, uts.machine, uts.release, uts.version);

	logti("With modules:");
	logti("  FFmpeg %s", GetFFmpegVersion());
	logti("    Configuration: %s", GetFFmpegConfiguration());
	logti("    libavformat: %s", GetFFmpegAvFormatVersion());
	logti("    libavcodec: %s", GetFFmpegAvCodecVersion());
	logti("    libavutil: %s", GetFFmpegAvUtilVersion());
	logti("    libavfilter: %s", GetFFmpegAvFilterVersion());
	logti("    libswresample: %s", GetFFmpegSwResampleVersion());
	logti("    libswscale: %s", GetFFmpegSwScaleVersion());
	logti("  SRT: %s", GetSrtVersion());
	logti("  SRTP: %s", GetSrtpVersion());
	logti("  OpenSSL: %s", GetOpenSslVersion());
	logti("    Configuration: %s", GetOpenSslConfiguration());
	logti("  JsonCpp: %s", GetJsonCppVersion());
	logti("  jemalloc: %s", GetJemallocVersion());
}