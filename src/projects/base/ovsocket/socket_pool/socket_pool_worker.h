//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../socket.h"
#include "../socket_datastructure.h"

namespace ov
{
	class SocketPool;

	class SocketEventInterface
	{
	};

	class SocketPoolWorker : public EnableSharedFromThis<SocketPoolWorker>
	{
	protected:
		friend class SocketPool;
		friend class Socket;

		OV_SOCKET_DECLARE_PRIVATE_TOKEN();

	public:
		// SocketPoolWorker can only be created within SocketPool
		SocketPoolWorker(PrivateToken token, const std::shared_ptr<SocketPool> &pool);
		~SocketPoolWorker() override;

		bool Initialize();
		bool Uninitialize();

		int GetNativeHandle() const;

		template <typename Tsocket = ov::Socket, typename... Targuments>
		std::shared_ptr<Tsocket> AllocSocket(Targuments... args)
		{
			auto socket = std::make_shared<Tsocket>(Socket::PrivateToken{nullptr}, this->GetSharedPtr(), args...);

			if (PrepareSocket(socket))
			{
				return socket;
			}

			return nullptr;
		}

		bool ReleaseSocket(const std::shared_ptr<Socket> &socket)
		{
			return RemoveFromEpoll(socket);
		}

	protected:
		SocketType GetType() const;

		static bool Compare(const std::shared_ptr<SocketPoolWorker> &worker1,
							const std::shared_ptr<SocketPoolWorker> &worker2)
		{
			return worker1->_socket_count < worker2->_socket_count;
		}

		bool PrepareSocket(std::shared_ptr<Socket> socket);

		void IncreaseSocketCount()
		{
			_socket_count++;
		}

		void DecreaseSocketCount()
		{
			_socket_count--;
			OV_ASSERT2(_socket_count >= 0);
		}

		bool PrepareEpoll();

		void MergeSocketList();
		void GarbageCollection();

		void ThreadProc();

		bool AttachToWorker(const std::shared_ptr<Socket> &socket);
		int EpollWait(int timeout_in_msec = Infinite);
		bool RemoveFromEpoll(const std::shared_ptr<Socket> &socket);

		void ConvertSrtEventToEpollEvent(const SRT_EPOLL_EVENT &srt_event, epoll_event *event);

		void EnqueueToDispatchLater(const std::shared_ptr<Socket> &socket);

		std::shared_ptr<SocketPool> _pool;

		// The number of sockets is determined in advance and managed separately for processing
		// Because excessive concentration may not be properly distributed to the worker,
		// the number of sockets can be specified in advance so that they can be distributed properly.
		std::atomic<int> _socket_count{0};
		// key: native handle of SocketWrapper
		std::map<int, std::shared_ptr<Socket>> _socket_map;
		// A list of sockets created/deleted from AttachToWorker()/RemoveFromEpoll()
		//
		// If RemoveFromEpoll() is called in thread #1 immediately after EpollWait() is called in thread #2,
		// Since there is no longer a reference to std::shared_ptr<Socket>, the socket will be released,
		// and after EpollWait() code will refer to the released socket.
		// To avoid this issue, make sure that _socket_map is modified only in ThreadProc().
		ov::Queue<std::shared_ptr<Socket>> _sockets_to_insert;
		ov::Queue<std::shared_ptr<Socket>> _sockets_to_delete;

		// Socket failed to send data for too long must be forced to shut down in the future
		ov::StopWatch _gc_interval;
		std::map<int, std::shared_ptr<Socket>> _gc_candidates;

		// Common variables
		std::thread _epoll_thread;
		bool _stop_epoll_thread = true;
		std::vector<epoll_event> _epoll_events;
		int _last_epoll_event_count = 0;

		// Occasionally, events on sockets need to be dispatched, even without events from epolls.
		// The queue used in this case
		std::mutex _sockets_to_dispatch_mutex;
		std::deque<std::shared_ptr<Socket>> _sockets_to_dispatch;

		// Related to epoll
		socket_t _epoll = InvalidSocket;

		// Related to SRT
		SRTSOCKET _srt_epoll = InvalidSocket;
		std::vector<SRT_EPOLL_EVENT> _srt_epoll_events;
	};

}  // namespace ov