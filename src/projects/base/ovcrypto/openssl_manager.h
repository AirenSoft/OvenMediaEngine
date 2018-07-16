//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

class OpenSSLManager
{
public:
	OpenSSLManager();
	virtual ~OpenSSLManager();

	static bool InitializeOpenSSL();
	static bool ReleaseOpenSSL();

private:
	static unsigned long GetThreadId();
	static void MutexLock(int mode, int n, const char *file, int line);

	static std::mutex *_mutex_array;
};
