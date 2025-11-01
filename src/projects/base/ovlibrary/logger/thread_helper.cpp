//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./thread_helper.h"

#include <spdlog/details/os.h>

#include "../assert.h"

namespace ov::logger
{
	ThreadHelper::ThreadHelper()
	{
		std::lock_guard lock(_thread_id_map_mutex);

#ifdef DEBUG
		_thread_id	= spdlog::details::os::thread_id();
		_pthread_id = ::pthread_self();
#endif	// DEBUG

		_thread_id_map[spdlog::details::os::thread_id()] = ::pthread_self();
	}

	ThreadHelper::~ThreadHelper()
	{
		std::lock_guard lock(_thread_id_map_mutex);

#if DEBUG
		auto thread_id	= spdlog::details::os::thread_id();
		auto pthread_id = ::pthread_self();

		if (thread_id != _thread_id)
		{
			OV_ASSERT(false, "thread_id mismatch in destructor. Expected: %zu, Actual: %zu", _thread_id, thread_id);
		}

		if (::pthread_equal(pthread_id, _pthread_id) == 0)
		{
			OV_ASSERT(false, "pthread_id mismatch in destructor. Expected: %lu, Actual: %lu", static_cast<unsigned long>(_pthread_id), static_cast<unsigned long>(pthread_id));
		}

		if (_thread_id_map.find(thread_id) == _thread_id_map.end())
		{
			OV_ASSERT(false, "Thread id not found in the map.");
		}
#endif	// DEBUG

		_thread_id_map.erase(spdlog::details::os::thread_id());
	}

	pthread_t ThreadHelper::GetPthreadId(size_t thread_id)
	{
		std::shared_lock lock(_thread_id_map_mutex);

		auto it = _thread_id_map.find(thread_id);

		if (it != _thread_id_map.end())
		{
			return it->second;
		}

#if DEBUG
		OV_ASSERT(false, "Thread id not found in the map.");
#endif	// DEBUG

		return 0;
	}

	std::unordered_map<size_t, pthread_t> ThreadHelper::_thread_id_map;
	std::shared_mutex ThreadHelper::_thread_id_map_mutex;
}  // namespace ov::logger
