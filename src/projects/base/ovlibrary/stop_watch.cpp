#include "stop_watch.h"

#include <utility>

#include "./log.h"

namespace ov
{
	StopWatch::StopWatch(String tag)
		: _tag(std::move(tag))
	{
	}

	void StopWatch::Start()
	{
		_is_valid = true;
		_start = std::chrono::high_resolution_clock::now();
		_last = _start;
	}

	bool StopWatch::Update()
	{
		_last = std::chrono::high_resolution_clock::now();
		return _is_valid;
	}

	int64_t StopWatch::Elapsed() const
	{
		if (_is_valid)
		{
			auto current = std::chrono::high_resolution_clock::now();

			return std::chrono::duration_cast<std::chrono::milliseconds>(current - _last).count();
		}

		return -1LL;
	}

	bool StopWatch::IsElapsed(int64_t milliseconds) const
	{
		return (Elapsed() >= milliseconds);
	}

	int64_t StopWatch::TotalElapsed() const
	{
		if (_is_valid)
		{
			auto current = std::chrono::high_resolution_clock::now();

			return std::chrono::duration_cast<std::chrono::milliseconds>(current - _start).count();
		}

		return -1LL;
	}

	void StopWatch::Print()
	{
		logd("StopWatch", "[%s] Elapsed: %lld (Total elapsed: %lld)", _tag.CStr(), Elapsed(), TotalElapsed());
	}
}  // namespace ov