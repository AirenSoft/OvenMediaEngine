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
		int64_t Elapsed() const;

		void Print();

	protected:
		String _tag;

		bool _is_valid { false };
		std::chrono::high_resolution_clock::time_point _start;
		std::chrono::high_resolution_clock::time_point _stop;
	};
}