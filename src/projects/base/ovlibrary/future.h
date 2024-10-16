//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <mutex>
#include <condition_variable>

namespace ov
{

	class Future
	{
	public:
		void Stop()
		{
			std::unique_lock<decltype(_mutex)> lock(_mutex);

			_stop_flag = true;

			_condition.notify_all();
		}

		// template <typename T>
		bool Submit(bool result)
		{
			std::unique_lock<decltype(_mutex)> lock(_mutex);

			_result = result;
			_condition.notify_all();

			return _result;
		}

		// template <typename T>
		bool Get()
		{
			std::unique_lock<decltype(_mutex)> lock(_mutex);

			_condition.wait(lock);

			if (_stop_flag)
			{
				return false;
			}

			return _result;
		}

		// return false : timed out, return true : signalled
		// template <typename T>
		bool GetFor(uint32_t timeout_delta_msec)
		{
			std::unique_lock<decltype(_mutex)> lock(_mutex);

			while (!_stop_flag)
			{
				auto result = _condition.wait_for(lock, std::chrono::milliseconds(timeout_delta_msec));
				if (result == std::cv_status::timeout)
				{
					return false;
				}
			}

			if (_stop_flag)
			{
				return false;
			}

			return _result;
		}

	private:
		std::mutex _mutex;
		std::condition_variable _condition;
		bool _stop_flag = false;
		bool _result = false;
	};
}