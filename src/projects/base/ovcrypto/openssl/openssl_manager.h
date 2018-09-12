//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace ov
{
	class OpensslManager
	{
	public:
		virtual ~OpensslManager() = default;

		static bool InitializeOpenssl();
		static bool ReleaseOpenSSL();

	protected:
		OpensslManager() = default;

	private:
		static unsigned long GetThreadId();
		static void MutexLock(int mode, int n, const char *file, int line);

		static std::mutex *_mutex_array;
	};
}
