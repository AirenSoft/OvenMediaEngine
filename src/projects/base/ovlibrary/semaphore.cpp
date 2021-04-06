//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./semaphore.h"
#include "./assert.h"

#include <chrono>

namespace ov
{
	void Semaphore::Notify()
	{
		std::unique_lock<decltype(_mutex)> lock(_mutex);

		++_count;

		_condition.notify_one();
	}

	void Semaphore::Wait()
	{
		std::unique_lock<decltype(_mutex)> lock(_mutex);

		while(_count <= 0)
		{
			_condition.wait(lock);
		}

		OV_ASSERT2(_count > 0);
		
		--_count;
	}

	bool Semaphore::WaitFor(uint32_t timeout_delta_msec)
	{
		std::unique_lock<decltype(_mutex)> lock(_mutex);

		while(_count <= 0)
		{
			auto result = _condition.wait_for(lock, std::chrono::milliseconds(timeout_delta_msec));
			if(result == std::cv_status::timeout)
			{
				return false;
			}
		}

		OV_ASSERT2(_count > 0);
		
		--_count;
		return true;
	}

	bool Semaphore::TryWait()
	{
		std::unique_lock<decltype(_mutex)> lock(_mutex);

		if(_count > 0)
		{
			--_count;
			return true;
		}
		return false;
	}
}