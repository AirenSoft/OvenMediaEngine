//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

#include "./dump_utilities.h"
#include "./log.h"
#include "./ovdata_structure.h"
#include "./stop_watch.h"
#include "./string.h"

namespace ov
{
	template <typename T>
	class Queue
	{
	public:
		Queue()
			: Queue(nullptr)
		{
		}

		Queue(const char *alias, size_t threshold = 0, int log_interval_in_msec = 5000)
			: _threshold(threshold),
			  _log_interval(log_interval_in_msec)
		{
			_queue_name.Format("Queue<%s>", Demangle(typeid(T).name()).CStr());

			if ((alias != nullptr) && (alias[0] == '\0'))
			{
				_queue_name.AppendFormat(" (%s)", alias);
			}

			_last_log_time.Start();

			logd("ov.Queue", "[%p] %s is created with threshold: %zu, interval: %d", this, _queue_name.CStr(), threshold, log_interval_in_msec);
		}

		~Queue()
		{
			logd("ov.Queue", "[%p] %s is destroyed", this, _queue_name.CStr());
		}

		void Enqueue(T &&item)
		{
			std::lock_guard<decltype(_mutex)> lock(_mutex);

			_queue.push(std::move(item));

			if ((_threshold > 0) && (_queue.size() >= _threshold))
			{
				if (_last_log_time.IsElapsed(_log_interval) && _last_log_time.Update())
				{
					logw("ov.Queue", "[%p] %s size has exceeded the threshold: queue: %zu, threshold: %zu", this, _queue_name.CStr(), _queue.size(), _threshold);
				}
			}

			_condition.notify_all();
		}

		// Timeout in milliseconds
		std::optional<T> Dequeue(int timeout = Infinite)
		{
			std::unique_lock<decltype(_mutex)> lock(_mutex);

			if (_stop)
			{
				return {};
			}

			std::chrono::system_clock::time_point expire =
				(timeout == Infinite) ? std::chrono::system_clock::time_point::max() : std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);

			if (_condition.wait_until(lock, expire, [this]() -> bool {
					return ((_queue.empty() == false) || _stop);
				}))
			{
				if (_stop == false)
				{
					T value = std::move(_queue.front());
					_queue.pop();

					return std::move(value);
				}
				else
				{
					// Stop is requested
				}
			}
			else
			{
				// timed out
			}

			return {};
		}

		bool IsEmpty() const
		{
			std::lock_guard<decltype(_mutex)> lock(_mutex);

			return _queue.empty();
		}

		void Clear()
		{
			std::lock_guard<decltype(_mutex)> lock(_mutex);

			// empty the queue
			_queue = {};
		}

		size_t Size() const
		{
			std::lock_guard<decltype(_mutex)> lock(_mutex);

			return _queue.size();
		}

		bool IsStopped() const
		{
			return _stop;
		}

		void Stop()
		{
			std::lock_guard<decltype(_mutex)> lock(_mutex);
			_stop = true;
			_condition.notify_all();
		}

	private:
		StopWatch _last_log_time;
		String _queue_name;
		size_t _threshold = 0;
		int _log_interval = 0;

		std::queue<T> _queue;
		mutable std::mutex _mutex;
		std::condition_variable _condition;
		bool _stop = false;
	};

}  // namespace ov