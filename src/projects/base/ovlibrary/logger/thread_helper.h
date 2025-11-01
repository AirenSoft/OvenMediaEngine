//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include <pthread.h>

#include <cstddef>
#include <shared_mutex>
#include <unordered_map>

namespace ov::logger
{
	constexpr size_t MAX_THREAD_NAME_LENGTH = 16;

	// This class maps spdlog's thread id to pthread id.
	// Usage: When a new thread is created, just declare this variable at the entry of the thread proc.
	// Upon declaration, the constructor adds the mapping, and later the destructor removes it by RAII.
	class ThreadHelper
	{
	public:
		ThreadHelper();
		~ThreadHelper();

		static pthread_t GetPthreadId(size_t thread_id);

	private:
		// key: `thread_id` - This is obtained by calling `thread_id()` of `spdlog`.
		// value: `pthread_id`
		static std::unordered_map<size_t, pthread_t> _thread_id_map;
		static std::shared_mutex _thread_id_map_mutex;

#ifdef DEBUG
		size_t _thread_id;
		pthread_t _pthread_id;
#endif	// DEBUG
	};
}  // namespace ov::logger
