#pragma once

#include "./string.h"

#include <chrono>
#include <mutex>
#include <atomic>

namespace ov
{
	class StopWatch
	{
	public:
		StopWatch() = default;
		explicit StopWatch(String tag);

		void Start();
		bool Update();
		int64_t Elapsed() const;
		bool IsElapsed(int64_t milliseconds) const;
		int64_t TotalElapsed() const;

		void Print();

	protected:
		String _tag;

		bool _is_valid { false };
		std::chrono::high_resolution_clock::time_point _start;
		std::chrono::high_resolution_clock::time_point _last;
		std::chrono::high_resolution_clock::time_point _stop;
	};
}