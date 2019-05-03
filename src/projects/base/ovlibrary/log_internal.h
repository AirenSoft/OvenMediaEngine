//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./string.h"
#include "./log.h"
#include "./assert.h"
#include "./log_write.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include <cstdio>
#include <ctime>
#include <chrono>
#include <regex>
#include <mutex>

#if DEBUG
#   define OV_LOG_SHOW_FILE_NAME                1
#   define OV_LOG_SHOW_FUNCTION_NAME            0
#else // DEBUG
#   define OV_LOG_SHOW_FILE_NAME                1
#   define OV_LOG_SHOW_FUNCTION_NAME            0
#endif // DEBUG

#define OV_LOG_COLOR_RESET                      "\x1B[0m"

#define OV_LOG_COLOR_FG_BLACK                   "\x1B[30m"
#define OV_LOG_COLOR_FG_RED                     "\x1B[31m"
#define OV_LOG_COLOR_FG_GREEN                   "\x1B[32m"
#define OV_LOG_COLOR_FG_YELLOW                  "\x1B[33m"
#define OV_LOG_COLOR_FG_BLUE                    "\x1B[34m"
#define OV_LOG_COLOR_FG_MAGENTA                 "\x1B[35m"
#define OV_LOG_COLOR_FG_CYAN                    "\x1B[36m"
#define OV_LOG_COLOR_FG_WHITE                   "\x1B[37m"

#define OV_LOG_COLOR_FG_BR_BLACK                "\x1B[90m"
#define OV_LOG_COLOR_FG_BR_RED                  "\x1B[91m"
#define OV_LOG_COLOR_FG_BR_GREEN                "\x1B[92m"
#define OV_LOG_COLOR_FG_BR_YELLOW               "\x1B[93m"
#define OV_LOG_COLOR_FG_BR_BLUE                 "\x1B[94m"
#define OV_LOG_COLOR_FG_BR_MAGENTA              "\x1B[95m"
#define OV_LOG_COLOR_FG_BR_CYAN                 "\x1B[96m"
#define OV_LOG_COLOR_FG_BR_WHITE                "\x1B[97m"

#define OV_LOG_COLOR_BG_BLACK                   "\x1B[40m"
#define OV_LOG_COLOR_BG_RED                     "\x1B[41m"
#define OV_LOG_COLOR_BG_GREEN                   "\x1B[42m"
#define OV_LOG_COLOR_BG_YELLOW                  "\x1B[43m"
#define OV_LOG_COLOR_BG_BLUE                    "\x1B[44m"
#define OV_LOG_COLOR_BG_MAGENTA                 "\x1B[45m"
#define OV_LOG_COLOR_BG_CYAN                    "\x1B[46m"
#define OV_LOG_COLOR_BG_WHITE                   "\x1B[47m"

#define OV_LOG_COLOR_BG_BR_BLACK                "\x1B[100m"
#define OV_LOG_COLOR_BG_BR_RED                  "\x1B[101m"
#define OV_LOG_COLOR_BG_BR_GREEN                "\x1B[102m"
#define OV_LOG_COLOR_BG_BR_YELLOW               "\x1B[103m"
#define OV_LOG_COLOR_BG_BR_BLUE                 "\x1B[104m"
#define OV_LOG_COLOR_BG_BR_MAGENTA              "\x1B[105m"
#define OV_LOG_COLOR_BG_BR_CYAN                 "\x1B[106m"
#define OV_LOG_COLOR_BG_BR_WHITE                "\x1B[107m"

namespace ov
{
	class LogInternal
	{
	public:
		LogInternal() noexcept
			: _level(OVLogLevelDebug)
		{
		}

		/// 모든 log에 1차적으로 적용되는 filter 규칙
		///
		/// @param level level 이상의 로그만 표시함
		inline void SetLogLevel(OVLogLevel level)
		{
			_level = level;
		}

		inline void ResetEnable()
		{
			std::lock_guard<std::mutex> lock(_mutex);

			_enable_map.clear();

			_enable_list.clear();
		}

		/// @param tag_regex tag 패턴
		/// @param level level 이상의 로그에 대해서만 is_enable 적용. level을 벗어나는 로그는 !is_enable로 간주함
		/// @param is_enabled 활성화 여부
		///
		/// @remarks 정규식 사용의 예: "App\..+" == "App.Hello", "App.World", "App.foo", "App.bar", ....
		///          (정규식에 대해서는 http://www.cplusplus.com/reference/regex/ECMAScript 참고)
		///          가장 먼저 입력된 tag_regex의 우선순위가 높음.
		///          동일한 tag에 대해, 다른 level이 설정되면 첫 번째로 설정한 level이 적용됨.
		///          예1) level이 debug 이면서 is_enabled가 false면, debug~critical 로그 출력 안함.
		///          예2) level이 warning 이면서 is_enabled가 false면, warning~critical 로그 출력 안함.
		///          예3) level이 warning 이면서 is_enabled가 true면, debug~information 로그 출력 안함, warning~critical 로그 출력함.
		///          예4) level이 info 이면서 is_enabled가 true, debug 로그 출력 안함, information~critical 로그 출력함.
		inline void SetEnable(const char *tag_regex, OVLogLevel level, bool is_enabled)
		{
			std::lock_guard<std::mutex> lock(_mutex);

			// 캐시 모두 삭제
			_enable_map.clear();

			_enable_list.emplace_back((EnableItem){
				.regex = std::make_shared<std::regex>(tag_regex),
				.level = level,
				.is_enabled = is_enabled
			});
		}

		inline int64_t GetThreadId()
		{
			return (int64_t)::syscall(SYS_gettid); // NOLINT
		}

		inline bool IsEnabled(const char *tag, OVLogLevel level)
		{
			std::lock_guard<std::mutex> lock(_mutex);

			auto item = _enable_map.find(tag);

			if(item == _enable_map.cend())
			{
				// 캐시된 항목에 없으면, _enable_list에서 정규식에 매칭되는 항목이 있는지 확인
				for(const auto &enable_item : _enable_list)
				{
					if(std::regex_match(tag, *(enable_item.regex.get())))
					{
						_enable_map[tag] = enable_item;

						break;
					}
				}

				item = _enable_map.find(tag);

				if(item == _enable_map.cend())
				{
					// 정규식에 매칭되는 항목이 없다면 info 이상의 로그 활성화
					_enable_map[tag] = (EnableItem){
						.regex = nullptr,
						.level = OVLogLevelInformation,
						.is_enabled = true
					};
				}

				item = _enable_map.find(tag);

				if(item == _enable_map.cend())
				{
					// 위에서 항목을 추가하였으므로, 절대로 여기로 진입하면 안됨
					OV_ASSERT2(false);
					return false;
				}
			}

			if(level >= item->second.level)
			{
				// 지정한 log level에 대해, 입력된 활성화 여부 반환
				return item->second.is_enabled;
			}

			// level 미만의 레벨은 활성화 여부와 반대로 동작
			return !item->second.is_enabled;
		}

		inline void Log(OVLogLevel level, const char *tag, const char *file, int line, const char *method, const char *format, va_list &arg_list)
		{
			if(level < _level)
			{
				// 비활성화 된 level
				return;
			}

			if(tag == nullptr)
			{
				tag = "";
			}

			if(IsEnabled(tag, level) == false)
			{
				// 비활성화 된 tag. 로그 출력 안함
				return;
			}

			const char *log_level[] = {
				"D",
				"I",
				"W",
				"E",
				"C"
			};

			const char *color_prefix[] = {
				OV_LOG_COLOR_FG_CYAN,
				OV_LOG_COLOR_FG_WHITE,
				OV_LOG_COLOR_FG_YELLOW,
				OV_LOG_COLOR_FG_BR_RED,
				OV_LOG_COLOR_FG_BR_WHITE OV_LOG_COLOR_BG_RED // NOLINT
			};

			const char *color_suffix[] = {
				OV_LOG_COLOR_RESET,
				OV_LOG_COLOR_RESET,
				OV_LOG_COLOR_RESET,
				OV_LOG_COLOR_RESET,
				OV_LOG_COLOR_RESET
			};

			// 현재 시각의 milliseconds를 얻어옴
			auto current = std::chrono::system_clock::now();
			auto mseconds = std::chrono::duration_cast<std::chrono::milliseconds>(current.time_since_epoch()).count() % 1000;

			// 시/분/초를 얻어옴
			std::time_t time = std::time(nullptr);
			std::tm localTime {};
			::localtime_r(&time, &localTime);

			ov::String log;

#if OV_LOG_SHOW_FILE_NAME
			ov::String fileName = file;

			{
				ssize_t position = fileName.IndexOfRev('/');

				if(position >= 0)
				{
					fileName = fileName.Substring(position + 1);
				}
			}
#endif // OV_LOG_SHOW_FILE_NAME

#if OV_LOG_SHOW_FUNCTION_NAME
			ov::String func = method;

	{
		int position = func.IndexOf('(');

		if(position >= 0)
		{
			func = func.Left(position);
		}

		position = func.IndexOfRev(' ');

		if(position >= 0)
		{
			func = func.Substring(position + 1);
		}
	}
#endif // OV_LOG_SHOW_FUNCTION_NAME

			// log format
			//  [<date> <time>] <tag> <log level> <thread id> | <message>
			log.Format(""
			           // color
			           "%s"
			           // date, time ([mm-dd hh:mm:ss.sss])
			           #if DEBUG
			           // DEBUG 모드일 땐 년도 표시 안함
			           "[%02d-%02d %02d:%02d:%02d.%03d]"
			           #else // DEBUG
			           // DEBUG 모드가 아닐 땐 년도 표시
		           "[%04d-%02d-%02d %02d:%02d:%02d.%03d]"
			           #endif // DEBUG
			           // <log level>
			           " %s"
			           // <thread id>
			           " %ld"
			           // tag
			           "%s%s"
			           // |
			           " | "
			           #if OV_LOG_SHOW_FILE_NAME
			           // File:Line
			           "%s:%-4d | "
#endif // OV_LOG_SHOW_FILE_NAME
#if OV_LOG_SHOW_FUNCTION_NAME
			// Method
				   "%s() | "
#endif // OV_LOG_SHOW_FUNCTION_NAME
				,
				       "",
#if DEBUG
                       localTime.tm_mon + 1, localTime.tm_mday,
#else // DEBUG
				1900 + localTime.tm_year, localTime.tm_mon + 1, localTime.tm_mday,
#endif // DEBUG
                       localTime.tm_hour, localTime.tm_min, localTime.tm_sec, mseconds,
                       log_level[level],
                       GetThreadId(),
                       (tag[0] == '\0') ? "" : " ", tag

#if OV_LOG_SHOW_FILE_NAME
				, fileName.CStr(), line
#endif // OV_LOG_SHOW_FILE_NAME
#if OV_LOG_SHOW_FUNCTION_NAME
				, func.CStr()
#endif // OV_LOG_SHOW_FUNCTION_NAME
			);

			// 맨 뒤에 <message> 추가
			log.AppendVFormat(format, &(arg_list[0]));

			if(level < OVLogLevelWarning)
			{
                fprintf(stdout, "%s%s%s\n", color_prefix[level], log.CStr(), color_suffix[level]);
				fflush(stdout);
			}
			else
			{
                fprintf(stderr, "%s%s%s\n", color_prefix[level], log.CStr(), color_suffix[level]);
                fflush(stderr);
			}

            _log_file.Write(log.CStr());
		}

        inline void SetLogPath(const char* log_path)
        {
            _log_file.SetLogPath(log_path);
        }

	protected:
		OVLogLevel _level;

		std::mutex _mutex;

        LogWrite _log_file;

		struct EnableItem
		{
			std::shared_ptr<std::regex> regex;
			OVLogLevel level;
			bool is_enabled;
		};

		std::vector<EnableItem> _enable_list;

		// 캐시 용도
		// key: tag
		// value: is_enabled
		std::map<ov::String, EnableItem> _enable_map;
	};
}