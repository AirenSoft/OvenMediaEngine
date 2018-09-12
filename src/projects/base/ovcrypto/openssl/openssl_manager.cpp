//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <mutex>
#include <thread>
#include <base/ovlibrary/ovlibrary.h>
#include "openssl_manager.h"
#include "openssl/ssl.h"

namespace ov
{
	std::mutex *OpensslManager::_mutex_array = nullptr;

	bool OpensslManager::InitializeOpenssl()
	{
		// Create mutexes for OpenSSL
		_mutex_array = new std::mutex[CRYPTO_num_locks()];

		if(_mutex_array == nullptr)
		{
			return false;
		}

		CRYPTO_set_id_callback(GetThreadId);
		CRYPTO_set_locking_callback(MutexLock);

		// Init OpenSSL
		::SSL_library_init();
		::SSL_load_error_strings();
		::ERR_load_BIO_strings();
		::OpenSSL_add_all_algorithms();

		return true;
	}

	bool OpensslManager::ReleaseOpenSSL()
	{
		CRYPTO_set_id_callback(nullptr);
		CRYPTO_set_locking_callback(nullptr);

		OV_SAFE_FUNC(_mutex_array, nullptr, delete[],);

		return true;
	}

	unsigned long OpensslManager::GetThreadId()
	{
		std::hash<std::thread::id> hasher;

		return hasher(std::this_thread::get_id());
	}

	void OpensslManager::MutexLock(int mode, int n, const char *file, int line)
	{
		if(mode & CRYPTO_LOCK)
		{
			_mutex_array[n].lock();
		}
		else
		{
			_mutex_array[n].unlock();
		}
	}
}