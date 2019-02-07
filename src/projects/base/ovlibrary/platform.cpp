//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "platform.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <zconf.h>
#include <pthread.h>

namespace ov
{
	std::string Platform::GetName()
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
}