//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "socket_pool_worker.h"

namespace ov
{
	class Socket;

	class SocketPool : public EnableSharedFromThis<SocketPool>
	{
	protected:
		OV_SOCKET_DECLARE_PRIVATE_TOKEN();

	public:
		// SocketPool can only be created using SocketPool::Create()
		SocketPool(PrivateToken token, const char *name, SocketType type);
		~SocketPool() override;

		static std::shared_ptr<SocketPool> Create(const char *name, SocketType type)
		{
			return std::make_shared<SocketPool>(PrivateToken{nullptr}, name, type);
		}

		static std::shared_ptr<SocketPool> GetTcpPool()
		{
			static std::shared_ptr<SocketPool> pool;
			static std::mutex mutex;

			if (pool == nullptr)
			{
				std::lock_guard lock_guard(mutex);

				if (pool == nullptr)
				{
					pool = Create("DefTcp", SocketType::Tcp);

					if (pool != nullptr)
					{
						pool->Initialize(1);
					}
				}
			}

			return pool;
		}

		static std::shared_ptr<SocketPool> GetUdpPool()
		{
			static std::shared_ptr<SocketPool> pool;
			static std::mutex mutex;

			if (pool == nullptr)
			{
				std::lock_guard lock_guard(mutex);

				if (pool == nullptr)
				{
					pool = Create("DefUdp", SocketType::Udp);

					if (pool != nullptr)
					{
						pool->Initialize(1);
					}
				}
			}

			return pool;
		}

		ov::String GetName() const
		{
			return _name;
		}

		SocketType GetType() const;

		bool Initialize(int worker_count);

		int GetWorkerCount() const
		{
			return static_cast<int>(_worker_list.size());
		}

		template <typename Tsocket = ov::Socket, typename... Targuments>
		std::shared_ptr<Tsocket> AllocSocket(Targuments... args)
		{
			std::shared_ptr<SocketPoolWorker> worker = GetIdleWorker();

			if (worker != nullptr)
			{
				auto socket = worker->AllocSocket<Tsocket>(args...);

				if (socket == nullptr)
				{
					// Rollback
					worker->DecreaseSocketCount();
				}

				return socket;
			}

			return nullptr;
		}

		bool ReleaseSocket(const std::shared_ptr<Socket> &socket)
		{
			return socket->GetSocketPoolWorker()->ReleaseSocket(socket);
		}

		bool Uninitialize();

	protected:
		// This method will increase the number of sockets for that worker by 1
		std::shared_ptr<SocketPoolWorker> GetIdleWorker()
		{
			std::lock_guard lock_guard(_worker_list_mutex);

			if (_worker_list.size() == 0)
			{
				OV_ASSERT(_worker_list.size() > 0, "Worker list is not initialized");
				return nullptr;
			}

			// Use the worker with the smallest number of sockets currently being processed
			auto worker = *std::min_element(_worker_list.begin(), _worker_list.end(), SocketPoolWorker::Compare);

			worker->IncreaseSocketCount();

			return worker;
		}

		bool UninitializeInternal();

		ov::String _name;

		SocketType _type = SocketType::Unknown;

		bool _initialized = false;

		std::mutex _worker_list_mutex;
		std::vector<std::shared_ptr<SocketPoolWorker>> _worker_list;

		std::mutex _socket_list_mutex;
		std::map<int, std::shared_ptr<Socket>> _socket_list;
	};
}  // namespace ov
