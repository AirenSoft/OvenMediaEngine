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
		name.Prepend("SP");
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

		_connection_callback_queue.Clear();

		_stop_epoll_thread = true;

		if (_epoll_thread.joinable())
		{
			_epoll_thread.join();
		}

		_socket_count = 0;
		{
			std::lock_guard lock_guard(_socket_map_mutex);
			_socket_map.clear();

			decltype(_sockets_to_insert)().swap(_sockets_to_insert);
			decltype(_sockets_to_delete)().swap(_sockets_to_delete);
		}

		{
			std::lock_guard lock_guard(_sockets_to_dispatch_mutex);
			_sockets_to_dispatch.clear();
		}

		_connection_timed_out_queue.clear();

		_gc_candidates.clear();

		OV_SAFE_FUNC(_epoll, InvalidSocket, ::close, );
		OV_SAFE_FUNC(_srt_epoll, InvalidSocket, ::srt_close, );

		return true;
	}

	int SocketPoolWorker::GetNativeHandle() const
	{
		return (GetType() == SocketType::Srt) ? _srt_epoll : _epoll;
	}

	SocketType SocketPoolWorker::GetType() const
	{
		return _pool->GetType();
	}

	bool SocketPoolWorker::PrepareEpoll()
	{
		logad("Creating epoll for %s...", StringFromSocketType(GetType()));

		std::shared_ptr<Error> error;

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
					srt_epoll_set(_srt_epoll, SRT_EPOLL_ENABLE_EMPTY);
					_epoll_events.resize(EpollMaxEvents);
					_srt_epoll_events.resize(EpollMaxEvents);
				}
				else
				{
					error = SrtError::CreateErrorFromSrt();
				}

				break;

			default:
				error = Error::CreateError("Socket", "Not implemented");
				break;
		}

		if (error != nullptr)
		{
			logae("Could not prepare epoll: %s (%s)",
				  error->What(),
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
		std::lock_guard lock_guard(_socket_map_mutex);

		decltype(_sockets_to_insert) insert_queue;
		decltype(_sockets_to_insert) delete_queue;

		std::swap(insert_queue, _sockets_to_insert);
		std::swap(delete_queue, _sockets_to_delete);
		while (insert_queue.empty() == false)
		{
			auto socket = insert_queue.front();
			insert_queue.pop();
			_socket_map[socket->GetNativeHandle()] = socket;
		}

		while (delete_queue.empty() == false)
		{
			auto socket = delete_queue.front();
			delete_queue.pop();
			_socket_map.erase(socket->GetNativeHandle());
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
				logaw("Failed to send data for %dms - This socket is going to be garbage collected (%s)", OV_SOCKET_EXPIRE_TIMEOUT, socket->ToString().CStr());

				DeleteFromEpoll(socket);
				socket->CloseImmediatelyWithState(SocketState::Disconnected);

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

	void SocketPoolWorker::CallbackTimedOutConnections()
	{
		if (_connection_timed_out_queue.size() <= 0)
		{
			return;
		}

		_connection_timed_out_queue_mutex.lock();
		auto timed_out_queue = std::move(_connection_timed_out_queue);
		_connection_timed_out_queue_mutex.unlock();

		auto socket_error = SocketError::CreateError("Connection timed out (by worker)");

		for (auto socket : timed_out_queue)
		{
			if (socket->GetState() == SocketState::Connecting)
			{
				socket->OnConnectedEvent(socket_error);
			}
		}
	}

	void SocketPoolWorker::ThreadProc()
	{
		_connection_callback_queue.Start();

		_gc_interval.Start();

		while (_stop_epoll_thread == false)
		{
			int count = EpollWait(100);

			if (count < 0)
			{
				logae("An error occurred - EpollWait()");
			}
			else
			{
				CallbackTimedOutConnections();

				for (int index = 0; index < count; index++)
				{
					auto &event = _epoll_events[index];

					auto socket_data = reinterpret_cast<Socket *>(event.data.ptr);

					if (socket_data == nullptr)
					{
						OV_ASSERT(socket_data != nullptr, "Could not convert event.data.ptr to ov::Socket *");
						logae("Could not convert socket data");
						continue;
					}

					auto socket = socket_data->GetSharedPtr();
					auto event_callback = socket_data->GetSharedPtrAs<SocketPoolEventInterface>();
					auto events = event.events;

					OV_ASSERT2(socket != nullptr);
					OV_ASSERT2(event_callback != nullptr);

					logad("Epoll event #%d (total: %d): %s, events: %s (%d, 0x%x), %s",
						  index, count,
						  socket->ToString().CStr(),
						  StringFromEpollEvent(event).CStr(), events, events,
						  Error::CreateErrorFromErrno()->What());

					if (socket->IsClosable() == false)
					{
						// The socket was closed or an error occurred just before this epoll events occurred.
						// So the socket can't receive the epoll events.
						logad("Epoll events are ignored - this event might occurs immediately after close/error");
						continue;
					}

					// Normal socket generates (EPOLLOUT | EPOLLHUP) events as soon as it is added to epoll
					// Client socket generates (EPOLLOUT | EPOLLIN) events as soon as it is added to epoll
					if (socket->NeedToWaitFirstEpollEvent())
					{
						if (OV_CHECK_FLAG(events, EPOLLOUT))
						{
							socket->SetFirstEpollEventReceived();

							// EPOLLOUT events might occur immediately after added to epoll
							logad("EPOLLOUT is ignored - this event might occurs immediately after added to epoll");

							continue;
						}

						OV_ASSERT(false, "EPOLLOUT event expected, but %s received", StringFromEpollEvent(event).CStr());
					}

					bool need_to_close = false;
					SocketState new_state = SocketState::Closed;

					if (OV_CHECK_FLAG(events, EPOLLOUT))
					{
						if (socket->GetState() == SocketState::Connecting)
						{
							int so_error = 0;

							if (socket->GetSockOpt(SO_ERROR, &so_error))
							{
								if (so_error == 0)
								{
									// Connected successfully
									event_callback->OnConnectedEvent(nullptr);
								}
								else
								{
									need_to_close = true;
									event_callback->OnConnectedEvent(SocketError::CreateError(so_error, "Socket error occurred: %s", ::strerror(so_error)));
								}
							}
							else
							{
								need_to_close = true;
								event_callback->OnConnectedEvent(SocketError::CreateError("Unknown error occurred: %s", StringFromEpollEvent(event).CStr()));
							}
						}
					}

					if (socket->GetBlockingMode() == BlockingMode::Blocking)
					{
						// Blocking mode handles only connection events
						continue;
					}

					if (need_to_close == false)
					{
						auto name = String(Platform::GetThreadName());

						if (OV_CHECK_FLAG(events, EPOLLOUT))
						{
							if (OV_CHECK_FLAG(events, EPOLLHUP) == false)
							{
								// Socket is ready to write data
								switch (event_callback->OnDataWritableEvent())
								{
									case PostProcessMethod::Nothing:
										break;

									case PostProcessMethod::GarbageCollection:
										logad("Need to do garbage collection for %s", socket->ToString().CStr());
										_gc_candidates[socket->GetNativeHandle()] = socket;
										break;

									case PostProcessMethod::Error:
										new_state = SocketState::Error;
										need_to_close = true;
										break;
								}
							}
							else
							{
								// EPOLLOUT can be ignored because it is not disconnected
								logtd("EPOLLOUT is received, but ignored by EPOLLHUP event");
							}
						}

						if (OV_CHECK_FLAG(events, EPOLLIN))
						{
							// Data is received from peer
							event_callback->OnDataAvailableEvent();
						}

						if (OV_CHECK_FLAG(events, EPOLLERR))
						{
							// An error occurred
							int socket_error;

							if (socket->IsClosable() && socket->GetSockOpt(SO_ERROR, &socket_error))
							{
								logad("EPOLLERR detected: %s\n", ::strerror(socket_error));
							}

							new_state = SocketState::Error;
							need_to_close = true;
						}

						if (OV_CHECK_FLAG(events, EPOLLHUP))
						{
							if (socket->GetState() != SocketState::Error)
							{
								// Disconnected
								socket->SetEndOfStream();

								new_state = SocketState::Disconnected;
							}
							else
							{
								new_state = SocketState::Error;
							}

							need_to_close = true;
						}
					}
					else
					{
						// An error occurred while connecting to remote
					}

					if (need_to_close)
					{
						_gc_candidates.erase(socket->GetNativeHandle());

						DeleteFromEpoll(socket);
						logad("CloseImmediatelyWithState(%s) for %s", StringFromSocketState(new_state), socket->ToString().CStr());
						socket->CloseImmediatelyWithState(new_state);
					}
				}
			}

			if (_sockets_to_dispatch.empty() == false)
			{
				// Move _extra_epoll_events to events to avoid blocking
				std::unordered_map<std::shared_ptr<Socket>, std::shared_ptr<Socket>> socket_list;

				{
					std::lock_guard lock_guard(_sockets_to_dispatch_mutex);
					std::swap(socket_list, _sockets_to_dispatch);
				}

				for (auto socket_item : socket_list)
				{
					auto socket = socket_item.second;

					switch (socket->DispatchEvents())
					{
						case Socket::DispatchResult::Dispatched:
							break;

						case Socket::DispatchResult::PartialDispatched:
							if (socket->IsClosable())
							{
								logad("Need to do garbage collection for %s (dispatch_later)", socket->ToString().CStr());
								_gc_candidates[socket->GetNativeHandle()] = socket;
							}
							else
							{
								// Socket is already closed
							}
							break;

						case Socket::DispatchResult::Error:
							if (socket->IsClosable())
							{
								logad("Socket %s will be closed by dispatcher", socket->ToString().CStr());
								socket->CloseImmediatelyWithState(SocketState::Error);
							}
							else
							{
								// Socket is already closed
							}
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

		_connection_callback_queue.Stop();

		// Clean up all sockets
		for (auto &socket_item : _socket_map)
		{
			auto socket = socket_item.second;

			// Close immediately (Do not half-close)
			if (socket->IsClosable())
			{
				socket->CloseImmediatelyWithState(SocketState::Closed);

				// Do connection callback, etc...
				socket->DispatchEvents();
			}
		}
	}

	bool SocketPoolWorker::AddToEpoll(const std::shared_ptr<Socket> &socket)
	{
		OV_ASSERT2(GetNativeHandle() != InvalidSocket);

		auto native_handle = socket->GetNativeHandle();
		std::shared_ptr<Error> error;

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
					error = SrtError::CreateErrorFromSrt();
				}

				break;
			}

			default:
				error = Error::CreateError("Socket", "Not implemented");
				break;
		}

		if (error == nullptr)
		{
			std::lock_guard lock_guard(_socket_map_mutex);
			_sockets_to_insert.push(socket);
		}
		else
		{
			logae("Could not add to epoll for descriptor %d (error: %s)", native_handle, error->What());
		}

		return (error == nullptr);
	}

	int SocketPoolWorker::EpollWait(int timeout_msec)
	{
		// Reset errno
		errno = 0;

		if (GetNativeHandle() == InvalidSocket)
		{
			logae("Epoll is not initialized");

			OV_ASSERT2(GetNativeHandle() != InvalidSocket);
			return -1;
		}

		std::shared_ptr<Error> error;
		int event_count = 0;

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				event_count = ::epoll_wait(_epoll, _epoll_events.data(), EpollMaxEvents, timeout_msec);

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
					error = Error::CreateErrorFromErrno();

					if (error->GetCode() == EINTR)
					{
						// Interruption of system calls and library functions by signal handlers
						event_count = 0;
						error = nullptr;
					}
					else
					{
						OV_ASSERT(false, "Unknown error: %s", error->What());
					}
				}

				break;

			case SocketType::Srt:
				event_count = ::srt_epoll_uwait(_srt_epoll, _srt_epoll_events.data(), EpollMaxEvents, timeout_msec);
				if (event_count == 0)
				{
					// timed out

					// https://github.com/Haivision/srt/blob/master/docs/API.md#srt_epoll_uwait
					// When the timeout is not -1, and no sockets are ready until the timeout time passes, this function returns 0. This behavior is different in srt_epoll_wait.
				}
				else if (event_count > 0)
				{
					// polled successfully
					MergeSocketList();

					// Make a list of epoll_event from SRT_EPOLL_EVENTs
					for (int index = 0; index < event_count; index++)
					{
						ConvertSrtEventToEpollEvent(_srt_epoll_events[index], &(_epoll_events[index]));
					}
				}
				else
				{
					error = SrtError::CreateErrorFromSrt();
					OV_ASSERT(false, "Unknown error: %s", error->What());
				}

				break;

			case SocketType::Unknown:
				error = Error::CreateError("Socket", "Unknown socket type: %s", StringFromSocketType(GetType()));
				break;
		}

		if (error == nullptr)
		{
			_last_epoll_event_count = event_count;
		}
		else
		{
			logae("Could not wait for epoll: %s", error->What());
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

		_sockets_to_dispatch[socket] = socket;
	}

	void SocketPoolWorker::EnqueueToCheckConnectionTimeOut(const std::shared_ptr<Socket> &socket, int timeout_msec)
	{
		_connection_callback_queue.Push(
			[=](void *parameter) -> DelayQueueAction {
				std::lock_guard lock_guard(_connection_timed_out_queue_mutex);
				_connection_timed_out_queue.push_back(socket);
				return DelayQueueAction::Stop;
			},
			nullptr,
			timeout_msec);
	}

	bool SocketPoolWorker::DeleteFromEpoll(const std::shared_ptr<Socket> &socket)
	{
		if (GetNativeHandle() == InvalidSocket)
		{
			logae("Epoll is not initialized");

			OV_ASSERT2(GetNativeHandle() != InvalidSocket);
			return false;
		}

		std::shared_ptr<Error> error;
		auto native_handle = socket->GetNativeHandle();

		logad("Trying to unregister a socket #%d from epoll...", native_handle);

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp: {
				if (::epoll_ctl(_epoll, EPOLL_CTL_DEL, native_handle, nullptr) == -1)
				{
					error = Error::CreateErrorFromErrno();
				}

				break;
			}

			case SocketType::Srt: {
				if (::srt_epoll_remove_usock(_srt_epoll, native_handle) == SRT_ERROR)
				{
					error = SrtError::CreateErrorFromSrt();
				}

				break;
			}

			default:
				error = Error::CreateError("Socket", "Not implemented");
				break;
		}

		if (error == nullptr)
		{
			logad("Socket #%d is unregistered", native_handle);
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
				logae("Could not delete the socket #%d from epoll: %s\n%s",
					  native_handle,
					  error->What(),
					  StackTrace::GetStackTrace().CStr());
			}
		}

		{
			std::lock_guard lock_guard(_socket_map_mutex);
			_socket_map.erase(socket->GetNativeHandle());
		}

		return (error == nullptr);
	}

	bool SocketPoolWorker::ReleaseSocket(const std::shared_ptr<Socket> &socket)
	{
		if (socket == nullptr)
		{
			return false;
		}

		return socket->Close();
	}

	String SocketPoolWorker::ToString() const
	{
		String description;

		description.AppendFormat(
			"<SocketPoolWorker: %p, socket_map: %zu, insert queue: %zu, delete queue: %zu, connection queue: %zu>",
			this, _socket_map.size(),
			_sockets_to_insert.size(), _sockets_to_delete.size(),
			_connection_timed_out_queue.size());

		return description;
	}

}  // namespace ov