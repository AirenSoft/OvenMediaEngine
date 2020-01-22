//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./third_parties.h"

#include <regex>

#include <base/ovcrypto/ovcrypto.h>
#include <srt/srt.h>

//--------------------------------------------------------------------
// Related to FFmpeg
//--------------------------------------------------------------------
extern "C"
{
#include <libavcodec/version.h>
#include <libavfilter/version.h>
#include <libavformat/version.h>
#include <libavutil/ffversion.h>
#include <libavutil/version.h>
#include <libswresample/version.h>
#include <libswscale/version.h>
}

const char *GetFFmpegConfiguration()
{
	return avutil_configuration();
}

const char *GetFFmpegVersion()
{
	return FFMPEG_VERSION;
}

const char *GetFFmpegAvFormatVersion()
{
	return AV_STRINGIFY(LIBAVFORMAT_VERSION);
}

const char *GetFFmpegAvCodecVersion()
{
	return AV_STRINGIFY(LIBAVCODEC_VERSION);
}

const char *GetFFmpegAvUtilVersion()
{
	return AV_STRINGIFY(LIBAVUTIL_VERSION);
}

const char *GetFFmpegAvFilterVersion()
{
	return AV_STRINGIFY(LIBAVFILTER_VERSION);
}

const char *GetFFmpegSwResampleVersion()
{
	return AV_STRINGIFY(LIBSWRESAMPLE_VERSION);
}

const char *GetFFmpegSwScaleVersion()
{
	return AV_STRINGIFY(LIBSWSCALE_VERSION);
}

//--------------------------------------------------------------------
// Related to SRTP
//--------------------------------------------------------------------
#include <srtp2/srtp.h>

const char *GetSrtpVersion()
{
	return srtp_get_version_string();
}

std::shared_ptr<ov::Error> InitializeSrtp()
{
	int err = ::srtp_init();

	if (err != srtp_err_status_ok)
	{
		return ov::Error::CreateError("SRTP", "Could not initialize SRTP (err: %d)", err);
	}

	return nullptr;
}

//--------------------------------------------------------------------
// Related to SRT
//--------------------------------------------------------------------
const char *GetSrtVersion()
{
	return SRT_VERSION_STRING;
}

static void SrtLogHandler(void *opaque, int level, const char *file, int line, const char *area, const char *message)
{
	// SRT log format:
	// HH:mm:ss.ssssss/SRT:xxxx:xxxxxx.N: xxx.c: .................
	// 13:20:15.618019.N: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317
	// 13:20:15.618019.!!FATAL!!: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317
	// 13:20:15.618019.!W: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317
	// 13:20:15.618019.*E: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317
	// 20:41:11.929158/ome_origin*E: SRT.d: SND-DROPPED 1 packets - lost delaying for 1021m
	// 13:20:15.618019/SRT:RcvQ:worker.N: SRT.c: PASSING request from: 192.168.0.212:63308 to agent:397692317

	std::smatch matches;
	std::string m = message;
	ov::String mess;

	if (std::regex_search(m, matches, std::regex("^([0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{6})([/a-zA-Z:.!*_]+) ([/a-zA-Z:.!]+ )?(.+)")))
	{
		mess = std::string(matches[4]).c_str();
	}
	else
	{
		// Unknown pattern
		mess = message;
	}

	const char *SRT_LOG_TAG = "SRT";

	switch (level)
	{
		case srt_logging::LogLevel::debug:
			ov_log_internal(OVLogLevelDebug, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case srt_logging::LogLevel::note:
			ov_log_internal(OVLogLevelInformation, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case srt_logging::LogLevel::warning:
			ov_log_internal(OVLogLevelWarning, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case srt_logging::LogLevel::error:
			ov_log_internal(OVLogLevelError, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		case srt_logging::LogLevel::fatal:
			ov_log_internal(OVLogLevelCritical, SRT_LOG_TAG, file, line, area, "%s", mess.CStr());
			break;

		default:
			ov_log_internal(OVLogLevelError, SRT_LOG_TAG, file, line, area, "(Unknown level: %d) %s", level, mess.CStr());
			break;
	}
}

std::shared_ptr<ov::Error> InitializeSrt()
{
	// https://github.com/Haivision/srt/blob/master/docs/API-functions.md#srt_startup
	// 0 = successfully run, or already started
	// 1 = this is the first startup, but the GC thread is already running
	// -1 = failed
	if (::srt_startup() == -1)
	{
		return ov::Error::CreateErrorFromSrt();
	}

	::srt_setloglevel(srt_logging::LogLevel::debug);
	::srt_setloghandler(nullptr, SrtLogHandler);

	return nullptr;
}

//--------------------------------------------------------------------
// Related to OpenSSL
//--------------------------------------------------------------------
const char *GetOpenSslConfiguration()
{
	return OpenSSL_version(OPENSSL_CFLAGS);
}

const char *GetOpenSslVersion()
{
	return OpenSSL_version(OPENSSL_VERSION);
}

std::shared_ptr<ov::Error> InitializeOpenSsl()
{
	if (ov::OpensslManager::InitializeOpenssl())
	{
		return nullptr;
	}

	return ov::Error::CreateErrorFromOpenSsl();
}
