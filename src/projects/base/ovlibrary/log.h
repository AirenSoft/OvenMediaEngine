//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

typedef enum OVLogLevel
{
	// 응용프로그램 분석을 위해 남기는 디버깅 로그
	// 예)
	// - 패킷/데이터 dump
	// - 함수 in/out 시점
	// - 메모리 할당/해제 시점
		OVLogLevelDebug,

	// 응용프로그램 전반적으로 발생하는 안내성 로그
	// 예)
	// - 응용프로그램이 시작/종료 되는 시점
	// - 서버 모듈이 시작/종료되는 시점
		OVLogLevelInformation,

	// 오류 상황이기는 하지만, 기능 동작에 영향을 미치지는 않는 경우 사용
	// 예)
	// - 외부에서 존재하지 않는 API가 호출되었거나 파라미터가 올바르지 않아 기능을 수행할 수 없을 때
	// - 접속이 되지 않아 재접속을 시도하는 경우
		OVLogLevelWarning,

	// 응용프로그램의 일부 기능이 아예 동작하지 않는 경우 사용
	// 예)
	// - 하드디스크 공간이 부족하여 데이터를 기록할 수 없는 경우
	// - 일시적인 네트워크 장애로 인해 TCP 연결이 끊겼을 경우
	// - 1935 port binding이 실패하여 RTMP 서버 모듈만 실행할 수 없는 경우
	// - 충분히 재접속을 시도하였으나 접속이 되지 않는 경우
		OVLogLevelError,

	// 응용프로그램이 더 이상 실행될 수 없는 상황이 되었을 때 사용
	// 예)
	// - 필수 환경 설정이 잘못되어 실행 불가능
	// - crash가 발생한 경우 (SIG handler에서 호출)
	// - 메모리 할당이 실패한 경우
	// - 스레드를 생성할 수 없는 경우
		OVLogLevelCritical
} OVLogLevel;

typedef enum StatLogType
{
	STAT_LOG_WEBRTC_EDGE_SESSION,
	STAT_LOG_WEBRTC_EDGE_REQUEST,
	STAT_LOG_WEBRTC_EDGE_VIEWERS,
	STAT_LOG_HLS_EDGE_SESSION,
	STAT_LOG_HLS_EDGE_REQUEST,
	STAT_LOG_HLS_EDGE_VIEWERS
} StatLogType;

#if DEBUG
#	define logd(tag, format, ...)                                                                                     \
		do                                                                                                             \
		{                                                                                                              \
			if (ov_log_get_enabled(tag, OVLogLevelDebug))                                                              \
			{                                                                                                          \
				ov_log_internal(OVLogLevelDebug, tag, __FILE__, __LINE__, __PRETTY_FUNCTION__, format, ##__VA_ARGS__); \
			}                                                                                                          \
		} while (false)

#	define logp(tag, format, ...)                     logd(tag ".Packet", format, ##__VA_ARGS__)
#else
#	define logd(...)                                  do {} while(false)
#	define logp                                       logd
#endif // DEBUG
//--------------------------------------------------------------------
// Logging APIs
//--------------------------------------------------------------------
#define logi(tag, format, ...)                        ov_log_internal(OVLogLevelInformation,    tag, __FILE__, __LINE__, __PRETTY_FUNCTION__, format, ## __VA_ARGS__)
#define logw(tag, format, ...)                        ov_log_internal(OVLogLevelWarning,        tag, __FILE__, __LINE__, __PRETTY_FUNCTION__, format, ## __VA_ARGS__)
#define loge(tag, format, ...)                        ov_log_internal(OVLogLevelError,          tag, __FILE__, __LINE__, __PRETTY_FUNCTION__, format, ## __VA_ARGS__)
#define logc(tag, format, ...)                        ov_log_internal(OVLogLevelCritical,       tag, __FILE__, __LINE__, __PRETTY_FUNCTION__, format, ## __VA_ARGS__)

//--------------------------------------------------------------------
// Logging APIs with tag
//--------------------------------------------------------------------
#define logtd(format, ...)                            logd(OV_LOG_TAG, format, ## __VA_ARGS__)
// for packet
#define logtp(format, ...)                            logp(OV_LOG_TAG, format, ## __VA_ARGS__)
// for statistics
#define logts(format, ...)                            logi(OV_LOG_TAG ".Stat", format, ## __VA_ARGS__)
#define logti(format, ...)                            logi(OV_LOG_TAG, format, ## __VA_ARGS__)
#define logtw(format, ...)                            logw(OV_LOG_TAG, format, ## __VA_ARGS__)
#define logte(format, ...)                            loge(OV_LOG_TAG, format, ## __VA_ARGS__)
#define logtc(format, ...)                            logc(OV_LOG_TAG, format, ## __VA_ARGS__)

#define stat_log(type, format, ...)                         ov_stat_log_internal(type, OVLogLevelInformation,    "STAT", __FILE__, __LINE__, __PRETTY_FUNCTION__, format, ## __VA_ARGS__)

/// 모든 log에 1차적으로 적용되는 filter 규칙
///
/// @param level level 이상의 로그만 표시함
void ov_log_set_level(OVLogLevel level);

void ov_log_reset_enable();

/// @param tag_regex tag 패턴
/// @param level level 이상의 로그에 대해서만 is_enable 적용. level을 벗어나는 로그는 !is_enable로 간주함
/// @param is_enabled 활성화 여부
///
/// @returns 성공적으로 적용되었는지 여부
///
/// @remarks 정규식 사용의 예: "App\..+" == "App.Hello", "App.World", "App.foo", "App.bar", ....
///          (정규식에 대해서는 http://www.cplusplus.com/reference/regex/ECMAScript 참고)
///          가장 먼저 입력된 tag_regex의 우선순위가 높음.
///          동일한 tag에 대해, 다른 level이 설정되면 첫 번째로 설정한 level이 적용됨.
///          예1) level이 debug 이면서 is_enabled가 false면, debug~critical 로그 출력 안함.
///          예2) level이 warning 이면서 is_enabled가 false면, warning~critical 로그 출력 안함.
///          예3) level이 warning 이면서 is_enabled가 true면, debug~information 로그 출력 안함, warning~critical 로그 출력함.
///          예4) level이 info 이면서 is_enabled가 true, debug 로그 출력 안함, information~critical 로그 출력함.
bool ov_log_set_enable(const char *tag_regex, OVLogLevel level, bool is_enabled);
bool ov_log_get_enabled(const char *tag, OVLogLevel level);

void ov_log_internal(OVLogLevel level, const char *tag, const char *file, int line, const char *method, const char *format, ...);
void ov_log_set_path(const char *log_path);

void ov_stat_log_internal(StatLogType type, OVLogLevel level, const char *tag, const char *file, int line, const char *method, const char *format, ...);
void ov_stat_log_set_path(StatLogType type, const char *log_path);


#ifdef __cplusplus
}
#endif // __cplusplus
