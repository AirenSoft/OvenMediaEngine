#pragma once

//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

template<typename T>
class MediaQueue
{
public:

	MediaQueue()
	{
		_abort = false;
	}

	T pop()
	{
		if(_abort)
		{
			return static_cast<T>(nullptr);
		}

		std::unique_lock<std::mutex> mlock(_mutex);
		while(_queue.empty())
		{
			_cond.wait(mlock);

			if(_abort)
			{
				return static_cast<T>(nullptr);
			}
		}

		auto item = _queue.front();
		_queue.pop();

		return item;
	}

	T pop_unique()
	{
		if(_abort)
		{
			return static_cast<T>(nullptr);
		}

		std::unique_lock<std::mutex> mlock(_mutex);
		while(_queue.empty())
		{
			_cond.wait(mlock);

			if(_abort)
			{
				return static_cast<T>(nullptr);
			}
		}

		auto item = std::move(_queue.front());

		_queue.pop();

		return item;
	}

	void pop(T &item)
	{
		std::unique_lock<std::mutex> mlock(_mutex);

		while(_queue.empty())
		{
			_cond.wait(mlock);
		}

		item = _queue.front();
		_queue.pop();
	}

	void push(const T &item)
	{
		std::unique_lock<std::mutex> mlock(_mutex);
		_queue.push(item);
		mlock.unlock();
		_cond.notify_one();
	}

	void push(T &&item)
	{
		std::unique_lock<std::mutex> mlock(_mutex);
		_queue.push(std::move(item));
		mlock.unlock();
		_cond.notify_one();
	}

	size_t size()
	{
		return _queue.size();
	}

	// TODO: Abort
	void abort()
	{
		_abort = true;
		_cond.notify_one();
	}

private:
	std::queue<T> _queue;
	std::mutex _mutex;
	std::condition_variable _cond;
	bool _abort;
};