#pragma once

#include "base/common_types.h"
#include <ctime>
#include <chrono>

namespace ov
{
	class Clock
	{
	public:
		Clock() = delete;
		~Clock() = delete;

		// yy:mm:dd HH:MM:SS.ms
		static ov::String Now()
		{
			auto now = std::chrono::system_clock::now();
			auto ttime_t = std::chrono::system_clock::to_time_t(now);
			auto tp_sec = std::chrono::system_clock::from_time_t(ttime_t);
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - tp_sec);

			std::tm * ttm = localtime(&ttime_t);
			char date_time_format[] = "%Y.%m.%d-%H.%M.%S";
			char time_str[] = "yyyy.mm.dd.HH-MM.SS.fff";

			strftime(time_str, strlen(time_str), date_time_format, ttm);

			ov::String result;
			result.AppendFormat("%s.%u", time_str, ms.count());

			return result;
		}
	};
}