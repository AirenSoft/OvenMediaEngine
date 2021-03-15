//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "socket_pool.h"

#include "../socket_private.h"

#define logad(format, ...) logtd("[%p] " format, this, ##__VA_ARGS__)
#define logas(format, ...) logts("[%p] " format, this, ##__VA_ARGS__)

#define logai(format, ...) logti("[%p] " format, this, ##__VA_ARGS__)
#define logaw(format, ...) logtw("[%p] " format, this, ##__VA_ARGS__)
#define logae(format, ...) logte("[%p] " format, this, ##__VA_ARGS__)
#define logac(format, ...) logtc("[%p] " format, this, ##__VA_ARGS__)

namespace ov
{
	SocketPool::SocketPool(PrivateToken token, const char *name, SocketType type)
		: _name(name),
		  _type(type)
	{
	}

	SocketPool::~SocketPool()
	{
		// Verify that the epoll is closed normally
		std::lock_guard lock_guard(_worker_list_mutex);

		for (auto &worker : _worker_list)
		{
			OV_ASSERT(worker->_epoll == InvalidSocket, "Epoll is not uninitialized");
		}
	}

	SocketType SocketPool::GetType() const
	{
		return _type;
	}

	bool SocketPool::Initialize(int worker_count)
	{
		if (_initialized)
		{
			logaw("Epoll is already initialized");
			return false;
		}

		if (worker_count < 0)
		{
			logae("Invalid worker count: %d", worker_count);
			OV_ASSERT2(false);
			return false;
		}

		if (worker_count > 1024)
		{
			logaw("Too many workers: %d, is this intended?", worker_count);
		}

		{
			std::lock_guard lock_guard(_worker_list_mutex);

			auto pool = GetSharedPtr();
			bool succeeded = true;

			logad("Trying to initialize socket pool with %d workers...", worker_count);

			for (int index = 0; index < worker_count; index++)
			{
				auto instance = std::make_shared<SocketPoolWorker>(SocketPoolWorker::PrivateToken{nullptr}, pool);

				if (instance->Initialize() == false)
				{
					succeeded = false;
					break;
				}

				_worker_list.emplace_back(instance);
			}

			if (succeeded)
			{
				logad("%d workers were created successfully", worker_count);
				_initialized = true;
			}
			else
			{
				logae("An error occurred while creating workers. Rollbacking...", worker_count);

				// Rollback
				UninitializeInternal();
			}
		}

		return _initialized;
	}

	bool SocketPool::UninitializeInternal()
	{
		for (auto worker : _worker_list)
		{
			worker->Uninitialize();
		}

		_worker_list.clear();

		return true;
	}

	bool SocketPool::Uninitialize()
	{
		logad("Trying to uninitialize socket pool...");

		std::lock_guard lock_guard(_worker_list_mutex);

		return UninitializeInternal();
	}
}  // namespace ov
