//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "platform.h"

#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <zconf.h>

namespace ov
{
	const char *Platform::GetName()
	{
		return PLATFORM_NAME;
	}

	uint64_t Platform::GetProcessId()
	{
		return static_cast<uint64_t>(getpid());
	}

	uint64_t Platform::GetThreadId()
	{
#if IS_MACOS
		uint64_t tid;
		::pthread_threadid_np(nullptr, &tid);
		return tid;
#elif IS_LINUX
		return static_cast<uint64_t>(::syscall(SYS_gettid));
#else
		return 0ULL;
#endif
	}

	const char *Platform::GetThreadName()
	{
		static thread_local char name[16]{0};
		static thread_local bool initialized = false;

		if (initialized == false)
		{
			::pthread_getname_np(::pthread_self(), name, 16);
			initialized = true;
		}

		return name;
	}
}  // namespace ov
