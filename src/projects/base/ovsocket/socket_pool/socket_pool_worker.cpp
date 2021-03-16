//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "socket_pool_worker.h"

#include "../socket_private.h"
#include "socket_pool.h"

#undef OV_LOG_TAG
#define OV_LOG_TAG "Socket.Pool.Worker"

#define logap(format, ...) logtp("[#%d] [%p] " format, (GetNativeHandle() == InvalidSocket) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logad(format, ...) logtd("[#%d] [%p] " format, (GetNativeHandle() == InvalidSocket) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logas(format, ...) logts("[#%d] [%p] " format, (GetNativeHandle() == InvalidSocket) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)

#define logai(format, ...) logti("[#%d] [%p] " format, (GetNativeHandle() == InvalidSocket) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logaw(format, ...) logtw("[#%d] [%p] " format, (GetNativeHandle() == InvalidSocket) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logae(format, ...) logte("[#%d] [%p] " format, (GetNativeHandle() == InvalidSocket) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logac(format, ...) logtc("[#%d] [%p] " format, (GetNativeHandle() == InvalidSocket) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)

#define SOCKET_POOL_WORKER_GC_INTERVAL 1000

namespace ov
{
	SocketPoolWorker::SocketPoolWorker(PrivateToken token, const std::shared_ptr<SocketPool> &pool)
		: _pool(pool)
	{
		OV_ASSERT2(_pool != nullptr);
	}

	SocketPoolWorker::~SocketPoolWorker()
	{
	}

	int SocketPoolWorker::GetNativeHandle() const
	{
		return (GetType() == SocketType::Srt) ? _srt_epoll : _epoll;
	}

	bool SocketPoolWorker::Initialize()
	{
		if (GetNativeHandle() != InvalidSocket)
		{
			logae("Epoll is already initialized (%s)", StringFromSocketType(GetType()));
			OV_ASSERT2(GetNativeHandle() == InvalidSocket);
			return false;
		}

		if (PrepareEpoll() == false)
		{
			return false;
		}

		_stop_epoll_thread = false;
		_epoll_thread = std::thread(&SocketPoolWorker::ThreadProc, this);

		auto name = _pool->GetName();
		name.Prepend("SckPool");
		name = name.Replace(" ", "");
		name.SetLength(15);

		::pthread_setname_np(_epoll_thread.native_handle(), name.CStr());

		return true;
	}

	bool SocketPoolWorker::Uninitialize()
	{
		if (GetNativeHandle() == InvalidSocket)
		{
			logae("Epoll is not initialized (%s)", StringFromSocketType(GetType()));
			OV_ASSERT2(GetNativeHandle() != InvalidSocket);
			return false;
		}

		_stop_epoll_thread = true;

		if (_epoll_thread.joinable())
		{
			_epoll_thread.join();
		}

		_socket_count = 0;
		_socket_map.clear();
		_sockets_to_insert.Clear();
		_sockets_to_dispatch.clear();
		_sockets_to_delete.Clear();

		_gc_candidates.clear();

		OV_SAFE_FUNC(_epoll, InvalidSocket, ::close, );
		OV_SAFE_FUNC(_srt_epoll, InvalidSocket, ::srt_close, );

		return true;
	}

	SocketType SocketPoolWorker::GetType() const
	{
		return _pool->GetType();
	}

	bool SocketPoolWorker::PrepareEpoll()
	{
		logad("Creating epoll for %s...", StringFromSocketType(GetType()));

		std::shared_ptr<ov::Error> error;

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				_epoll = ::epoll_create1(0);

				if (_epoll != InvalidSocket)
				{
					_epoll_events.resize(EpollMaxEvents);
				}
				else
				{
					error = Error::CreateErrorFromErrno();
				}

				break;

			case SocketType::Srt:
				_srt_epoll = ::srt_epoll_create();

				if (_srt_epoll != SRT_INVALID_SOCK)
				{
					_epoll_events.resize(EpollMaxEvents);
					_srt_epoll_events.resize(EpollMaxEvents);
				}
				else
				{
					error = Error::CreateErrorFromSrt();
				}

				break;

			default:
				error = ov::Error::CreateError("Socket", "Not implemented");
				break;
		}

		if (error != nullptr)
		{
			logae("Could not prepare epoll: %s (%s)",
				  error->ToString().CStr(),
				  StringFromSocketType(GetType()));
		}
		else
		{
			logad("Epoll is created for %s",
				  StringFromSocketType(GetType()));
		}

		return (error == nullptr);
	}

	bool SocketPoolWorker::PrepareSocket(std::shared_ptr<Socket> socket)
	{
		return socket->Create(GetType());
	}

	void SocketPoolWorker::MergeSocketList()
	{
		while (_sockets_to_insert.IsEmpty() == false)
		{
			auto socket_value = _sockets_to_insert.Dequeue();

			if (socket_value.has_value())
			{
				auto socket = socket_value.value();

				_socket_map[socket->GetNativeHandle()] = socket;
			}
		}

		while (_sockets_to_delete.IsEmpty() == false)
		{
			auto socket_value = _sockets_to_delete.Dequeue();

			if (socket_value.has_value())
			{
				auto socket = socket_value.value();

				_socket_map.erase(socket->GetNativeHandle());
			}
		}
	}

	void SocketPoolWorker::GarbageCollection()
	{
		auto candidate = _gc_candidates.begin();

		while (candidate != _gc_candidates.end())
		{
			auto socket = candidate->second;

			if (socket->HasExpiredCommand())
			{
				// Sockets that have failed to send data for a long time are forced to shut down
				logaw("Failed to send data for %dms - This socket is going to be garbage collection (%s)", OV_SOCKET_EXPIRE_TIMEOUT, socket->ToString().CStr());

				RemoveFromEpoll(socket);

				candidate = _gc_candidates.erase(candidate);
			}
			else if (socket->HasCommand() == false)
			{
				// There have been unprocessed commands in the past, but now all of them have been processed
				logad("All commands of socket are processed (%s)", socket->ToString().CStr());
				candidate = _gc_candidates.erase(candidate);
			}
			else
			{
				candidate++;
			}
		}
	}

	void SocketPoolWorker::ThreadProc()
	{
		_gc_interval.Start();

		while (_stop_epoll_thread == false)
		{
			int count = EpollWait(200);

			if (count < 0)
			{
				logae("An error occurred");
			}
			else
			{
				for (int index = 0; index < count; index++)
				{
					auto &event = _epoll_events[index];

					auto socket_data = reinterpret_cast<ov::Socket *>(event.data.ptr);

					if (socket_data == nullptr)
					{
						OV_ASSERT(socket_data != nullptr, "Could not convert event.data.ptr to ov::Socket *");
						logae("Could not convert socket data");
						continue;
					}

					auto socket = socket_data->GetSharedPtr();
					auto events = event.events;

					if (OV_CHECK_FLAG(events, EPOLLOUT))
					{
						switch (socket->DispatchEvents())
						{
							case Socket::DispatchResult::Dispatched:
								break;

							case Socket::DispatchResult::PartialDispatched:
								_gc_candidates[socket->GetNativeHandle()] = socket;
								break;

							case Socket::DispatchResult::Error:
								socket->Close();
								break;
						}
					}

					if (OV_CHECK_FLAG(events, EPOLLIN))
					{
						socket->OnReadableFromSocket();
					}

					if (OV_CHECK_FLAG(events, EPOLLHUP) || OV_CHECK_FLAG(events, EPOLLRDHUP))
					{
						auto state = socket->GetState();

						if ((state == SocketState::Created) || (state == SocketState::Listening))
						{
							// EPOLLHUP event might occur immediately after creation
						}
						else
						{
							// Disconnected
							logad("Epoll event raised: %s, events: %s (%d, 0x%x), %s",
								  socket->ToString().CStr(),
								  StringFromEpollEvent(event).CStr(), events, events,
								  ov::Error::CreateErrorFromErrno()->ToString().CStr());

							socket->SetEndOfStream();
							if (socket->GetState() != SocketState::Closed)
							{
								socket->SetState(SocketState::Disconnected);
							}
							socket->CloseIfNeeded();

							_gc_candidates.erase(socket->GetNativeHandle());
						}
					}
				}
			}

			if (_sockets_to_dispatch.empty() == false)
			{
				// Move _extra_epoll_events to events to avoid blocking
				std::deque<std::shared_ptr<Socket>> socket_list;

				{
					std::lock_guard lock_guard(_sockets_to_dispatch_mutex);
					std::swap(socket_list, _sockets_to_dispatch);
				}

				for (auto &socket : socket_list)
				{
					switch (socket->DispatchEvents())
					{
						case Socket::DispatchResult::Dispatched:
							break;

						case Socket::DispatchResult::PartialDispatched:
							_gc_candidates[socket->GetNativeHandle()] = socket;
							break;

						case Socket::DispatchResult::Error:
							socket->Close();
							break;
					}
				}
			}

			if (_gc_interval.IsElapsed(SOCKET_POOL_WORKER_GC_INTERVAL) && _gc_interval.Update())
			{
				GarbageCollection();
			}

			MergeSocketList();
		}

		// Clean up all sockets
		for (auto &socket_item : _socket_map)
		{
			auto socket = socket_item.second;

			// Close immediately
			socket->CloseInternal();
		}
	}

	bool SocketPoolWorker::AttachToWorker(const std::shared_ptr<Socket> &socket)
	{
		OV_ASSERT2(GetNativeHandle() != InvalidSocket);

		auto native_handle = socket->GetNativeHandle();
		std::shared_ptr<ov::Error> error;

		switch (GetType())
		{
			case SocketType::Tcp:
			case SocketType::Udp: {
				epoll_event event{};

				// EPOLLIN: input event
				// EPOLLOUT: output event
				// EPOLLERR: error event
				// EPOLLHUP: hang up
				// EPOLLPRI: for urgent data (OOB)
				// EPOLLRDHUP : Disconnected or half-closing
				// EPOLLET: Edge trigger
				event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLET;
				event.data.ptr = socket.get();

				logad("Trying to add socket #%d to epoll...", native_handle);

				if (::epoll_ctl(_epoll, EPOLL_CTL_ADD, native_handle, &event) == -1)
				{
					error = Error::CreateErrorFromErrno();
				}

				break;
			}

			case SocketType::Srt: {
				int events = SRT_EPOLL_IN | SRT_EPOLL_OUT | SRT_EPOLL_ERR | SRT_EPOLL_ET;

				if (::srt_epoll_add_usock(_srt_epoll, native_handle, &events) == SRT_ERROR)
				{
					error = Error::CreateErrorFromSrt();
				}

				break;
			}

			default:
				error = ov::Error::CreateError("Socket", "Not implemented");
				break;
		}

		if (error == nullptr)
		{
			_sockets_to_insert.Enqueue(socket);
		}
		else
		{
			logae("Could not add to epoll for descriptor %d (error: %s)", native_handle, error->ToString().CStr());
		}

		return (error == nullptr);
	}

	int SocketPoolWorker::EpollWait(int timeout_in_msec)
	{
		if (GetNativeHandle() == InvalidSocket)
		{
			logae("Epoll is not initialized");

			OV_ASSERT2(GetNativeHandle() != InvalidSocket);
			return -1;
		}

		std::shared_ptr<ov::Error> error;
		int event_count = 0;

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				event_count = ::epoll_wait(_epoll, _epoll_events.data(), EpollMaxEvents, timeout_in_msec);

				if (event_count == 0)
				{
					// timed out
				}
				else if (event_count > 0)
				{
					// polled successfully
				}
				else
				{
					error = ov::Error::CreateErrorFromErrno();

					if (error->GetCode() == EINTR)
					{
						// Interruption of system calls and library functions by signal handlers
						event_count = 0;
						error = nullptr;
					}
					else
					{
						OV_ASSERT(false, "Unknown error: %s", error->ToString().CStr());
					}
				}

				break;

			case SocketType::Srt:
				event_count = ::srt_epoll_uwait(_srt_epoll, _srt_epoll_events.data(), EpollMaxEvents, timeout_in_msec);

				if (event_count == 0)
				{
					// timed out

					// https://github.com/Haivision/srt/blob/master/docs/API.md#srt_epoll_uwait
					// When the timeout is not -1, and no sockets are ready until the timeout time passes, this function returns 0. This behavior is different in srt_epoll_wait.
				}
				else if (event_count > 0)
				{
					// polled successfully

					// Make a list of epoll_event from SRT_EPOLL_EVENTs
					for (int index = 0; index < event_count; index++)
					{
						ConvertSrtEventToEpollEvent(_srt_epoll_events[index], &(_epoll_events[index]));
					}
				}
				else
				{
					OV_ASSERT(false, "Unknown error");

					error = ov::Error::CreateErrorFromSrt();
				}

				break;

			case SocketType::Unknown:
				error = ov::Error::CreateError("Socket", "Unknown socket type: %s", StringFromSocketType(GetType()));
				break;
		}

		if (error == nullptr)
		{
			_last_epoll_event_count = event_count;
		}
		else
		{
			logae("Could not wait for epoll: %s", error->ToString().CStr());
			_last_epoll_event_count = 0;
		}

		return _last_epoll_event_count;
	}

	void SocketPoolWorker::ConvertSrtEventToEpollEvent(const SRT_EPOLL_EVENT &srt_event, epoll_event *event)
	{
		SRTSOCKET srt_socket = srt_event.fd;
		SRT_SOCKSTATUS status = ::srt_getsockstate(srt_socket);

		event->data.ptr = _socket_map[srt_socket].get();
		event->events = 0;

		if (OV_CHECK_FLAG(srt_event.events, SRT_EPOLL_IN))
		{
			event->events |= EPOLLIN;
		}
		if (OV_CHECK_FLAG(srt_event.events, SRT_EPOLL_OUT))
		{
			event->events |= EPOLLOUT;
		}
		if (OV_CHECK_FLAG(srt_event.events, SRT_EPOLL_ERR))
		{
			event->events |= EPOLLERR;
		}

		switch (status)
		{
			case SRTS_LISTENING:
				// New SRT client connection
				break;

			case SRTS_NONEXIST:
				event->events |= EPOLLHUP;
				break;

			case SRTS_BROKEN:
				// The client is disconnected (unexpected)
				event->events |= EPOLLHUP;
				break;

			case SRTS_CLOSED:
				// The client is disconnected (expected)
				event->events |= EPOLLHUP;
				break;

			case SRTS_CONNECTED:
				// A client is connected
				break;

			default:
				logad("Not handled SRT status %d for socket #%d", status, srt_socket);
				break;
		}
	}

	void SocketPoolWorker::EnqueueToDispatchLater(const std::shared_ptr<Socket> &socket)
	{
		std::lock_guard lock_guard(_sockets_to_dispatch_mutex);

		_sockets_to_dispatch.push_back(socket);
	}

	bool SocketPoolWorker::RemoveFromEpoll(const std::shared_ptr<Socket> &socket)
	{
		if (GetNativeHandle() == InvalidSocket)
		{
			logae("Epoll is not initialized");

			OV_ASSERT2(GetNativeHandle() != InvalidSocket);
			return false;
		}

		std::shared_ptr<ov::Error> error;
		auto native_handle = socket->GetNativeHandle();

		logad("Trying to remove a socket #%d from epoll...", native_handle);

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp: {
				if (::epoll_ctl(_epoll, EPOLL_CTL_DEL, native_handle, nullptr) == -1)
				{
					error = ov::Error::CreateErrorFromErrno();
				}

				break;
			}

			case SocketType::Srt: {
				if (::srt_epoll_remove_usock(_srt_epoll, native_handle) == SRT_ERROR)
				{
					error = ov::Error::CreateErrorFromSrt();
				}

				break;
			}

			default:
				error = ov::Error::CreateError("Socket", "Not implemented");
				break;
		}

		if (error == nullptr)
		{
			_sockets_to_delete.Enqueue(socket);

			socket->CloseInternal();

			logad("Removed socket: #%d", native_handle);
		}
		else
		{
			if (error->GetCode() == EBADF)
			{
				// Socket is closed somewhere in OME

				// Do not print 'Bad file descriptor' error log
			}
			else
			{
				logae("Could not delete the socket %d from epoll: %s\n%s",
					  native_handle,
					  error->ToString().CStr(),
					  ov::StackTrace::GetStackTrace().CStr());
			}
		}

		return (error == nullptr);
	}
}  // namespace ov