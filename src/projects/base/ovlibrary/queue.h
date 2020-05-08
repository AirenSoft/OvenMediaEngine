//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./ovdata_structure.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <optional>

namespace ov
{
	template <typename T>
	class Queue
	{
	public:
		void Enqueue(T &&item)
		{
			std::lock_guard<decltype(_mutex)> lock(_mutex);

			_queue.push(std::move(item));
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
		std::queue<T> _queue;
		mutable std::mutex _mutex;
		std::condition_variable _condition;
		bool _stop = false;
	};

}  // namespace ov