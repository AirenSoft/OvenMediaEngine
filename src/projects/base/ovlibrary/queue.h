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

		T Dequeue(int timeout = Infinite)
		{
			T item{};

			if (Dequeue(&item, timeout))
			{
				return std::move(item);
			}

			return item;
		}

		// Timeout in milliseconds
		bool Dequeue(T *item, int timeout = Infinite)
		{
			std::unique_lock<decltype(_mutex)> lock(_mutex);

			std::chrono::system_clock::time_point expire =
				(timeout == Infinite) ? std::chrono::system_clock::time_point::max() : std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);

			if (_condition.wait_until(lock, expire, [this]() -> bool {
					return ((_queue.empty() == false) || _stop);
				}))
			{
				if (_stop == false)
				{
					*item = std::move(_queue.front());
					_queue.pop();

					return true;
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

			return false;
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

		void Notify()
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