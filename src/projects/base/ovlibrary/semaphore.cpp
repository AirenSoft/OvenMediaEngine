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

		--_count;
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