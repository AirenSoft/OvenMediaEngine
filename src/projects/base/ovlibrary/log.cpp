//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "log.h"
#include "log_internal.h"

//TODO(Getroot): This is temporary code for testing. This will change to more elegant code in the future.
static ov::LogInternal g_log_internal(OV_LOG_FILE);
static ov::LogInternal g_stat_wrs_log_internal(OV_STAT_WRS_LOG_FILE);
static ov::LogInternal g_stat_wrr_log_internal(OV_STAT_WRR_LOG_FILE);
static ov::LogInternal g_stat_wrv_log_internal(OV_STAT_WRV_LOG_FILE);
static ov::LogInternal g_stat_hrs_log_internal(OV_STAT_HRS_LOG_FILE);
static ov::LogInternal g_stat_hrr_log_internal(OV_STAT_HRR_LOG_FILE);
static ov::LogInternal g_stat_hrv_log_internal(OV_STAT_HRV_LOG_FILE);

// log level 지정
void ov_log_set_level(OVLogLevel level)
{
	g_log_internal.SetLogLevel(level);
}

void ov_log_reset_enable()
{
	g_log_internal.ResetEnable();
}

// tag는 정규식 사용 가능, 정규식에 대해서는 http://www.cplusplus.com/reference/regex/ECMAScript 참고
bool ov_log_set_enable(const char *tag_regex, OVLogLevel level, bool is_enabled)
{
	return g_log_internal.SetEnable(tag_regex, level, is_enabled);
}

bool ov_log_get_enabled(const char *tag, OVLogLevel level)
{
	return g_log_internal.IsEnabled(tag, level);
}

void ov_log_internal(OVLogLevel level, const char *tag, const char *file, int line, const char *method, const char *format, ...)
{
	va_list arg_list;
	va_start(arg_list, format);

	g_log_internal.Log(true, level, tag, file, line, method, format, arg_list);

	va_end(arg_list);
}

void ov_log_set_path(const char *log_path)
{
    g_log_internal.SetLogPath(log_path);
}

void ov_stat_log_internal(StatLogType type, OVLogLevel level, const char *tag, const char *file, int line, const char *method, const char *format, ...)
{
	va_list arg_list;
	va_start(arg_list, format);

	switch(type)
	{
		case STAT_LOG_WEBRTC_EDGE_SESSION:
			g_stat_wrs_log_internal.Log(false, level, tag, file, line, method, format, arg_list);
			break;
		case STAT_LOG_WEBRTC_EDGE_REQUEST:
			g_stat_wrr_log_internal.Log(false, level, tag, file, line, method, format, arg_list);
			break;
		case STAT_LOG_WEBRTC_EDGE_VIEWERS:
			g_stat_wrv_log_internal.Log(false, level, tag, file, line, method, format, arg_list);
			break;
		case STAT_LOG_HLS_EDGE_SESSION:
			g_stat_hrs_log_internal.Log(false, level, tag, file, line, method, format, arg_list);
			break;
		case STAT_LOG_HLS_EDGE_REQUEST:
			g_stat_hrr_log_internal.Log(false, level, tag, file, line, method, format, arg_list);
			break;
		case STAT_LOG_HLS_EDGE_VIEWERS:
			g_stat_hrv_log_internal.Log(false, level, tag, file, line, method, format, arg_list);
			break;
	}

	va_end(arg_list);
}

void ov_stat_log_set_path(StatLogType type, const char *log_path)
{
	switch(type)
	{
		case STAT_LOG_WEBRTC_EDGE_SESSION:
			g_stat_wrs_log_internal.SetLogPath(log_path);
			break;
		case STAT_LOG_WEBRTC_EDGE_REQUEST:
			g_stat_wrr_log_internal.SetLogPath(log_path);
			break;
		case STAT_LOG_WEBRTC_EDGE_VIEWERS:
			g_stat_wrv_log_internal.SetLogPath(log_path);
			break;
		case STAT_LOG_HLS_EDGE_SESSION:
			g_stat_hrs_log_internal.SetLogPath(log_path);
			break;
		case STAT_LOG_HLS_EDGE_REQUEST:
			g_stat_hrr_log_internal.SetLogPath(log_path);
			break;
		case STAT_LOG_HLS_EDGE_VIEWERS:
			g_stat_hrv_log_internal.SetLogPath(log_path);
			break;
	}
}