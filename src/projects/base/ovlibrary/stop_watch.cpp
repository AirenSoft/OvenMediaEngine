#include "stop_watch.h"
#include "./log.h"

#include <utility>

namespace ov
{
	StopWatch::StopWatch(String tag)
		: _tag(std::move(tag))
	{
	}

	void StopWatch::Start()
	{
		_start = std::chrono::high_resolution_clock::now();
		_is_valid = true;
	}

	int64_t StopWatch::Elapsed() const
	{
		if(_is_valid)
		{
			auto current = std::chrono::high_resolution_clock::now();
			return std::chrono::duration_cast<std::chrono::milliseconds>(current - _start).count();
		}

		return -1LL;
	}

	void StopWatch::Print()
	{
		logd("StopWatch", "[%s] Elapsed: %lld", _tag.CStr(), Elapsed());
	}
}