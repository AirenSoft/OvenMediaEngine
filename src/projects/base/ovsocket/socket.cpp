//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>

#include "epoll_wrapper.h"
#include "socket_pool/socket_pool.h"
#include "socket_private.h"
#include "socket_utilities.h"

// Debugging purpose
#include "stats_counter.h"

#define logap(format, ...) logtp("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logad(format, ...) logtd("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logas(format, ...) logts("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)

#define logai(format, ...) logti("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logaw(format, ...) logtw("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logae(format, ...) logte("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logac(format, ...) logtc("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)

#define USE_SOCKET_PROFILER 0

namespace ov
{
#if USE_SOCKET_PROFILER
	// Calculate and callback the time before and after the mutex lock and the time until the method is completely processed

	class SocketProfiler
	{
	public:
		using PostHandler = std::function<void(int64_t lock_elapsed, int64_t total_elapsed)>;

		SocketProfiler()
		{
			sw.Start();
		}

		~SocketProfiler()
		{
			if (post_handler != nullptr)
			{
				post_handler(lock_elapsed, sw.Elapsed());
			}
		}

		void AfterLock()
		{
			lock_elapsed = sw.Elapsed();
		}

		void SetPostHandler(PostHandler post_handler)
		{
			this->post_handler = post_handler;
		}

		StopWatch sw;
		int64_t lock_elapsed;
		PostHandler post_handler;
	};

#	define SOCKET_PROFILER_INIT() SocketProfiler __socket_profiler
#	define SOCKET_PROFILER_AFTER_LOCK() __socket_profiler.AfterLock()
#	define SOCKET_PROFILER_POST_HANDLER(handler) __socket_profiler.SetPostHandler(handler)
#else  // USE_SOCKET_PROFILER
#	define SOCKET_PROFILER_NOOP() \
		do                         \
		{                          \
		} while (false)

#	define SOCKET_PROFILER_INIT() SOCKET_PROFILER_NOOP()
#	define SOCKET_PROFILER_AFTER_LOCK() SOCKET_PROFILER_NOOP()
#	define SOCKET_PROFILER_POST_HANDLER(handler) SOCKET_PROFILER_NOOP()
#endif	// USE_SOCKET_PROFILER

	// Used to wait for connection
	class ConnectHelper : public SocketAsyncInterface
	{
	public:
		void OnConnected(const std::shared_ptr<const SocketError> &error) override
		{
			_epoll_event.SetEvent();
			_error = error;
		}

		void OnReadable() override
		{
		}

		void OnClosed() override
		{
		}

		bool WaitForConnect(int timeout)
		{
			return _epoll_event.Wait(timeout);
		}

		std::shared_ptr<const SocketError> GetError() const
		{
			return _error;
		}

	protected:
		Event _epoll_event{true};
		std::shared_ptr<const SocketError> _error;
	};

	Socket::Socket(PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker)
		: _worker(worker)
	{
		OV_ASSERT(worker != nullptr, "Worker must be not nullptr");
	}

	// Creates a socket using remote information.
	Socket::Socket(PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker,
				   SocketWrapper socket, const SocketAddress &remote_address)
		: Socket(token, worker)
	{
		_socket = socket;

		// Remote information is present, considered already connected
		SetState(SocketState::Connected);

		_remote_address = std::make_shared<SocketAddress>(remote_address);
	}

	Socket::~Socket()
	{
		// Verify that the socket is closed normally
		OV_ASSERT(_socket.IsValid() == false, "Socket is not closed. Current state: %s", StringFromSocketState(GetState()));
		CHECK_STATE2(== SocketState::Closed, >= SocketState::Disconnected, );
	}

	bool Socket::Create(SocketType type)
	{
		CHECK_STATE(== SocketState::Closed, false);

		if (_socket.IsValid())
		{
			logae("Socket is already created: %d", GetNativeHandle());
			return false;
		}

		logad("Trying to create new socket (type: %d)...", type);

		switch (type)
		{
			case SocketType::Tcp:
			case SocketType::Udp:
				_socket.SetSocket(type, ::socket(PF_INET, (type == SocketType::Tcp) ? SOCK_STREAM : SOCK_DGRAM, 0));
				break;

			case SocketType::Srt:
				_socket.SetSocket(type, ::srt_create_socket());
				break;

			default:
				break;
		}

		do
		{
			if (_socket.IsValid() == false)
			{
				logae("An error occurred while create socket");
				break;
			}

			logad("Socket descriptor is created for type %s", StringFromSocketType(type));

			_has_close_command = false;
			_end_of_stream = false;

			_connection_event_fired = false;

			SetState(SocketState::Created);

			return true;
		} while (false);

		{
			std::lock_guard lock_guard(_dispatch_queue_lock);
			if (CloseInternal())
			{
				SetState(SocketState::Closed);
			}
		}

		return false;
	}

	bool Socket::SetBlockingInternal(BlockingMode mode)
	{
		if (_socket.IsValid() == false)
		{
			logae("Could not make %s socket (Invalid socket)", StringFromBlockingMode(mode));
			OV_ASSERT2(_socket.IsValid());
			return false;
		}

		auto socket = GetSharedPtr();

		switch (GetType())
		{
			case SocketType::Tcp:
			case SocketType::Udp: {
				int result = ::fcntl(GetNativeHandle(), F_GETFL, 0);

				if (result == -1)
				{
					logae("Could not obtain flags: %s", Error::CreateErrorFromErrno()->What());
					return false;
				}

				int flags = (mode == BlockingMode::Blocking) ? (result & ~O_NONBLOCK) : (result | O_NONBLOCK);

				result = ::fcntl(GetNativeHandle(), F_SETFL, flags);

				if (result == -1)
				{
					logae("Could not set flags: %s", Error::CreateErrorFromErrno()->What());
					return false;
				}

				break;
			}

			case SocketType::Srt:
				if (SetSockOpt(SRTO_RCVSYN, mode == BlockingMode::Blocking) == false || SetSockOpt(SRTO_SNDSYN, mode == BlockingMode::Blocking) == false)
				{
					logae("Could not set flags to SRT socket %d: %s", GetNativeHandle(), SrtError::CreateErrorFromSrt()->What());
					return false;
				}

				break;

			default:
				OV_ASSERT(false, "Invalid socket type: %d", GetType());
				return false;
		}

		return true;
	}

	bool Socket::AppendCommand(DispatchCommand command)
	{
		SOCKET_PROFILER_INIT();
		std::lock_guard lock_guard(_dispatch_queue_lock);
		SOCKET_PROFILER_AFTER_LOCK();

		SOCKET_PROFILER_POST_HANDLER([&](int64_t lock_elapsed, int64_t total_elapsed) {
			if ((lock_elapsed > 100) || (_dispatch_queue.size() > 10))
			{
				logtw("[SockProfiler] AppendCommand() - %s, Queue: %zu, Lock: %dms, Total: %dms", ToString().CStr(), _dispatch_queue.size(), lock_elapsed, total_elapsed);
			}
		});

		if (_has_close_command)
		{
			// Socket was closed
			return false;
		}

		_dispatch_queue.push_back(std::move(command));

		return true;
	}

	bool Socket::AddToWorker(bool need_to_wait_first_epoll_event)
	{
		if (GetType() == SocketType::Srt)
		{
			// SRT doesn't generates any epoll events after ::srt_epoll_add_usock()
			need_to_wait_first_epoll_event = false;
		}

		if (need_to_wait_first_epoll_event)
		{
			ResetFirstEpollEventReceived();
		}

		if (_worker->AddToEpoll(GetSharedPtr()))
		{
			if (need_to_wait_first_epoll_event)
			{
				return WaitForFirstEpollEvent();
			}

			return true;
		}

		return false;
	}

	bool Socket::DeleteFromWorker()
	{
		return _worker->DeleteFromEpoll(GetSharedPtr());
	}

	bool Socket::MakeBlocking()
	{
		if (GetState() != SocketState::Created)
		{
			logae("Blocking mode can only be changed at the beginning of socket creation");
			OV_ASSERT2(false);
			return false;
		}

		if (_blocking_mode == BlockingMode::Blocking)
		{
			// Socket is already blocking mode
			OV_ASSERT2(_callback == nullptr);

			return true;
		}

		auto old_callback = _callback;
		_callback = nullptr;

		if (
			SetBlockingInternal(BlockingMode::Blocking) &&
			DeleteFromWorker())
		{
			_blocking_mode = BlockingMode::Blocking;
			return true;
		}

		// Rollback
		_callback = old_callback;

		return false;
	}

	bool Socket::MakeNonBlocking(std::shared_ptr<SocketAsyncInterface> callback)
	{
		if (GetState() != SocketState::Created)
		{
			logae("Blocking mode can only be changed at the beginning of socket creation");
			OV_ASSERT2(false);
			return false;
		}

		return MakeNonBlockingInternal(callback, true);
	}

	bool Socket::MakeNonBlockingInternal(std::shared_ptr<SocketAsyncInterface> callback, bool need_to_wait_first_epoll_event)
	{
		if (_blocking_mode == BlockingMode::NonBlocking)
		{
			// Socket is already non-blocking mode
			_callback = callback;

			return true;
		}

		auto old_callback = _callback;

		_callback = callback;
		_blocking_mode = BlockingMode::NonBlocking;

		if (
			SetBlockingInternal(BlockingMode::NonBlocking) &&
			AddToWorker(need_to_wait_first_epoll_event))
		{
			return true;
		}

		// Rollback
		_callback = old_callback;
		_blocking_mode = BlockingMode::Blocking;

		return false;
	}

	bool Socket::Bind(const SocketAddress &address)
	{
		CHECK_STATE(== SocketState::Created, false);

		if (_socket.IsValid() == false)
		{
			logae("Could not bind socket (Invalid socket)");
			OV_ASSERT2(_socket.IsValid());
			return false;
		}

		logad("Binding to %s...", address.ToString().CStr());

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp: {
				int result = ::bind(GetNativeHandle(), address.Address(), static_cast<socklen_t>(address.AddressLength()));

				if (result == 0)
				{
					// 성공
					_local_address = std::make_shared<SocketAddress>(address);
				}
				else
				{
					// 실패
					logae("Could not bind to %s (%d)", address.ToString().CStr(), result);
					return false;
				}

				break;
			}

			case SocketType::Srt: {
				int result = ::srt_bind(GetNativeHandle(), address.Address(), static_cast<int>(address.AddressLength()));

				if (result != SRT_ERROR)
				{
					// 성공
					_local_address = std::make_shared<SocketAddress>(address);
				}
				else
				{
					// 실패
					logae("Could not bind to %s for SRT (%s)", address.ToString().CStr(), srt_getlasterror_str());
					return false;
				}

				break;
			}

			default:
				OV_ASSERT(false, "Invalid socket type: %d", GetType());
				return false;
		}

		SetState(SocketState::Bound);
		logad("Bound successfully");

		return true;
	}

	bool Socket::Listen(int backlog)
	{
		CHECK_STATE(== SocketState::Bound, false);

		switch (GetType())
		{
			case SocketType::Tcp: {
				int result = ::listen(GetNativeHandle(), backlog);
				if (result == 0)
				{
					// 성공
					SetState(SocketState::Listening);
					return true;
				}

				logae("Could not listen: %s", Error::CreateErrorFromErrno()->What());
				break;
			}

			case SocketType::Srt: {
				int result = ::srt_listen(GetNativeHandle(), backlog);
				if (result != SRT_ERROR)
				{
					SetState(SocketState::Listening);
					return true;
				}

				logae("Could not listen: %s", srt_getlasterror_str());
				break;
			}

			default:
				OV_ASSERT(false, "Invalid socket type: %d", GetType());
				break;
		}

		return false;
	}

	SocketWrapper Socket::Accept(SocketAddress *client)
	{
		CHECK_STATE(<= SocketState::Listening, SocketWrapper());

		switch (GetType())
		{
			case SocketType::Tcp: {
				sockaddr_in client_addr{};
				socklen_t client_length = sizeof(client_addr);

				socket_t client_socket = ::accept(GetNativeHandle(), reinterpret_cast<sockaddr *>(&client_addr), &client_length);

				if (client_socket != InvalidSocket)
				{
					*client = SocketAddress(client_addr);
				}

				return SocketWrapper(GetType(), client_socket);
			}

			case SocketType::Srt: {
				sockaddr_storage client_addr{};
				int client_length = sizeof(client_addr);

				SRTSOCKET client_socket = ::srt_accept(GetNativeHandle(), reinterpret_cast<sockaddr *>(&client_addr), &client_length);

				if (client_socket != SRT_INVALID_SOCK)
				{
					*client = SocketAddress(client_addr);
				}

				return SocketWrapper(GetType(), client_socket);
			}

			default:
				OV_ASSERT(false, "Invalid socket type: %d", GetType());
				break;
		}

		return SocketWrapper();
	}

	std::shared_ptr<const SocketError> Socket::DoConnectionCallback(const std::shared_ptr<const SocketError> &error)
	{
		if (_blocking_mode == BlockingMode::Blocking)
		{
			if (error == nullptr)
			{
				SetState(SocketState::Connected);
			}
			else
			{
				CloseWithState(SocketState::Error);
			}

			return error;
		}

		OnConnectedEvent(error);

		return nullptr;
	}

	std::shared_ptr<const SocketError> Socket::Connect(const SocketAddress &endpoint, int timeout_msec)
	{
		OV_ASSERT2(_socket.IsValid());

		CHECK_STATE(== SocketState::Created, DoConnectionCallback(SocketError::CreateError(EINVAL, "Invalid state: %d", static_cast<int>(_state))));

		if (endpoint.IsValid() == false)
		{
			return DoConnectionCallback(SocketError::CreateError("Invalid address: %s", endpoint.ToString().CStr()));
		}

		std::shared_ptr<const SocketError> socket_error;
		std::shared_ptr<ConnectHelper> connect_helper;

		// To set connection timeout
		bool use_timeout = (timeout_msec > 0 && timeout_msec < Infinite);

		switch (GetType())
		{
			case SocketType::Tcp:
			case SocketType::Udp: {
				if (_blocking_mode == BlockingMode::Blocking)
				{
					if (use_timeout)
					{
						if (
							(SetBlockingInternal(BlockingMode::NonBlocking) == false) ||
							(AddToWorker(true) == false))
						{
							return DoConnectionCallback(SocketError::CreateError("Socket cannot be changed to nonblocking mode: %s", ToString().CStr()));
						}

						connect_helper = std::make_shared<ConnectHelper>();
						_callback = connect_helper;
					}
				}

				SetState(SocketState::Connecting);

				logad("Trying to connect to %s...", endpoint.ToString().CStr());
				int result = ::connect(GetNativeHandle(), endpoint.Address(), endpoint.AddressLength());

				if (result == 0)
				{
					if (_blocking_mode == BlockingMode::Blocking)
					{
						_callback = nullptr;

						if (use_timeout)
						{
							// Restore blocking state
							if (
								(DeleteFromWorker() == false) ||
								(SetBlockingInternal(BlockingMode::Blocking) == false))
							{
								return DoConnectionCallback(SocketError::CreateError("Socket cannot be changed to blocking mode: %s", ToString().CStr()));
							}
						}
					}

					return DoConnectionCallback(nullptr);
				}

				if (result < 0)
				{
					auto error = Error::CreateErrorFromErrno();

					// Check timeout
					if (error->GetCode() == EINPROGRESS)
					{
						if (_blocking_mode == BlockingMode::NonBlocking)
						{
							// Don't wait for the connection if this socket is non-blocking mode
							_worker->EnqueueToCheckConnectionTimeOut(GetSharedPtr(), timeout_msec);
							return nullptr;
						}

						// Blocking mode
						if (connect_helper->WaitForConnect(timeout_msec))
						{
							socket_error = connect_helper->GetError();
						}
						else
						{
							// Timed out
							int so_error;
							GetSockOpt(SO_ERROR, &so_error);

							socket_error = SocketError::CreateError(so_error, "Connection timed out (%d ms elapsed, SO_ERROR: %d)", timeout_msec, so_error);
							logad("%s", socket_error->What());
						}

						// Restore blocking state
						if (
							(DeleteFromWorker() == false) ||
							(SetBlockingInternal(BlockingMode::Blocking) == false))
						{
							socket_error = (socket_error == nullptr) ? SocketError::CreateError("Socket cannot be changed to blocking mode: %s", ToString().CStr()) : socket_error;
						}
					}
					else
					{
						socket_error = SocketError::CreateError(error);
					}
				}

				break;
			}

			case SocketType::Srt:
				if (SetSockOpt(SRTO_CONNTIMEO, timeout_msec))
				{
					if (::srt_connect(GetNativeHandle(), endpoint.Address(), endpoint.AddressLength()) != SRT_ERROR)
					{
						return DoConnectionCallback(nullptr);
					}
				}

				socket_error = SocketError::CreateError(SrtError::CreateErrorFromSrt());

				break;

			default:
				socket_error = SocketError::CreateError("Not implemented");
				break;
		}

		return DoConnectionCallback(socket_error);
	}

	bool Socket::SetRecvTimeout(const timeval &tv)
	{
		OV_ASSERT2(_socket.IsValid());
		//CHECK_STATE(== SocketState::Connected, false);

		switch (GetType())
		{
			case SocketType::Tcp:
			case SocketType::Udp:
				return SetSockOpt(SO_RCVTIMEO, tv);

			case SocketType::Srt:
			default:
				OV_ASSERT(false, "Not implemented");
				return false;
		}
	}

	std::shared_ptr<SocketAddress> Socket::GetLocalAddress() const
	{
		return _local_address;
	}

	std::shared_ptr<SocketAddress> Socket::GetRemoteAddress() const
	{
		return _remote_address;
	}

	String Socket::GetRemoteAddressAsUrl() const
	{
		return String::FormatString("%s://%s", StringFromSocketType(GetType()), GetRemoteAddress() != nullptr ? GetRemoteAddress()->ToString(false).CStr() : "unknown");
	}

	bool Socket::SetSockOpt(int proto, int option, const void *value, socklen_t value_length)
	{
		CHECK_STATE(!= SocketState::Closed, false);

		int result = ::setsockopt(GetNativeHandle(), proto, option, value, value_length);

		if (result != 0)
		{
			logaw("Could not set option: %d (result: %d)", option, result);
			return false;
		}

		return true;
	}

	bool Socket::SetSockOpt(int option, const void *value, socklen_t value_length)
	{
		return SetSockOpt(SOL_SOCKET, option, value, value_length);
	}

	bool Socket::GetSockOpt(int proto, int option, void *value, socklen_t value_length) const
	{
		CHECK_STATE(!= SocketState::Closed, false);

		int result = ::getsockopt(GetNativeHandle(), proto, option, value, &value_length);

		if (result != 0)
		{
			logaw("Could not get option: %d (result: %d)", option, result);
			return false;
		}

		return true;
	}

	bool Socket::SetSockOpt(SRT_SOCKOPT option, const void *value, int value_length)
	{
		CHECK_STATE(!= SocketState::Closed, false);

		int result = ::srt_setsockopt(GetNativeHandle(), 0, option, value, value_length);

		if (result == SRT_ERROR)
		{
			auto error = SrtError::CreateErrorFromSrt();
			logaw("Could not set option: %d (result: %s)", option, error->What());
			return false;
		}

		return true;
	}

	bool Socket::IsClosable() const
	{
		return OV_CHECK_FLAG(ToUnderlyingType(_state), SOCKET_STATE_CLOSABLE);
	}

	SocketState Socket::GetState() const
	{
		return _state;
	}

	void Socket::SetState(SocketState state)
	{
		logad("Socket state is changed: %s => %s",
			  StringFromSocketState(_state),
			  StringFromSocketState(state));

		_state = state;
	}

	SocketType Socket::GetType() const
	{
		return _socket.GetType();
	}

	// Only avaliable if socket is SRT
	String Socket::GetStreamId() const
	{
		return _stream_id;
	}

	bool Socket::OnConnectedEvent(const std::shared_ptr<const SocketError> &error)
	{
		if (error == nullptr)
		{
			SetState(SocketState::Connected);

			if (_callback != nullptr)
			{
				_connection_event_fired = true;
				_callback->OnConnected(error);
			}
		}
		else
		{
			auto callback = std::move(_callback);

			CloseWithState(SocketState::Error);

			if (callback != nullptr)
			{
				_connection_event_fired = true;
				callback->OnConnected(error);
			}
		}

		return true;
	}

	Socket::DispatchResult Socket::DispatchEventInternal(DispatchCommand &command)
	{
		SOCKET_PROFILER_INIT();
		SOCKET_PROFILER_POST_HANDLER([&](int64_t lock_elapsed, int64_t total_elapsed) {
			if (total_elapsed > 100)
			{
				logtw("[SockProfiler] DispatchEventInternal() - %s, Total: %dms", ToString().CStr(), total_elapsed);
			}
		});

		ssize_t sent_bytes = 0;
		auto &data = command.data;

		logap("Dispatching event: %s", command.ToString().CStr());

		switch (command.type)
		{
			case DispatchCommand::Type::Connected:
				if (OnConnectedEvent(nullptr))
				{
					return DispatchResult::Dispatched;
				}

				return DispatchResult::Error;

			case DispatchCommand::Type::Send:
				sent_bytes = SendInternal(data);
				break;

			case DispatchCommand::Type::SendTo:
				sent_bytes = SendToInternal(command.address, data);
				break;

			case DispatchCommand::Type::HalfClose:
				return HalfClose();

			case DispatchCommand::Type::WaitForHalfClose:
				return WaitForHalfClose();

			case DispatchCommand::Type::Close: {
				logad("Trying to close the socket...");

				// Remove the socket from epoll
				auto result = _worker->DeleteFromEpoll(this->GetSharedPtr());

				CloseInternal();

				if (result == false)
				{
					SetState(SocketState::Error);
					return DispatchResult::Error;
				}

				SetState(command.new_state);
				return DispatchResult::Dispatched;
			}
		}

		if (sent_bytes == static_cast<ssize_t>(command.data->GetLength()))
		{
			return DispatchResult::Dispatched;
		}

		if (sent_bytes == -1)
		{
			return DispatchResult::Error;
		}

		if (sent_bytes > 0)
		{
			// Since some data has been sent, the time needs to be updated.
			command.UpdateTime();
			data = data->Subdata(sent_bytes);

			logad("Part of the data has been sent: %ld bytes, left: %ld bytes (%s)", sent_bytes, data->GetLength(), command.ToString().CStr());
		}
		else
		{
			// logad("Could not send data: %ld bytes (%s)", data->GetLength(), command.ToString().CStr());
		}

		return DispatchResult::PartialDispatched;
	}

	Socket::DispatchResult Socket::DispatchEventsInternal()
	{
		SOCKET_PROFILER_INIT();

		DispatchResult result = DispatchResult::Dispatched;

		{
			SOCKET_PROFILER_AFTER_LOCK();

			std::lock_guard lock_guard(_dispatch_queue_lock);

			[[maybe_unused]] auto count = _dispatch_queue.size();
			SOCKET_PROFILER_POST_HANDLER([&](int64_t lock_elapsed, int64_t total_elapsed) {
				if ((lock_elapsed > 100) || (count > 10) || (_dispatch_queue.size() > 10))
				{
					logtw("[SockProfiler] DispatchEventsInternal() - %s, Before Queue: %zu, After Queue: %zu, Lock: %dms, Total: %dms",
						  ToString().CStr(), count, _dispatch_queue.size(), lock_elapsed, total_elapsed);
				}
			});

			if (_dispatch_queue.empty() == false)
			{
				logap("Dispatching events (count: %zu)...", _dispatch_queue.size());

				while (_dispatch_queue.empty() == false)
				{
					auto front = _dispatch_queue.front();
					_dispatch_queue.pop_front();

					bool is_close_command = front.IsCloseCommand();

					if ((GetState() == SocketState::Closed) && (is_close_command == false))
					{
						// If the socket is closed during dispatching, the rest of the data will not be sent.
						logad("Some commands have not been dispatched: %zu commands", _dispatch_queue.size());
#if DEBUG
						for (auto &queue : _dispatch_queue)
						{
							logad("  - Command: %s", queue.ToString().CStr());
						}
#endif	// DEBUG

						_dispatch_queue.clear();

						result = DispatchResult::Dispatched;
						break;
					}

					result = DispatchEventInternal(front);

					if (result == DispatchResult::Dispatched)
					{
						// Dispatches the next item
						continue;
					}
					else if (result == DispatchResult::PartialDispatched)
					{
						// The data is not fully processed and will not be removed from queue

						_dispatch_queue.emplace_front(front);

						// Close-related commands will be processed when we receive the event from epoll later
					}
					else
					{
						// An error occurred
						if (is_close_command)
						{
							// Ignore errors that occurred during close
							result = DispatchResult::Dispatched;
							continue;
						}
					}

					break;
				}
			}
		}

		// Since the resource is usually cleaned inside the OnClosed() callback,
		// callback is performed outside the lock_guard to prevent acquiring the lock.
		auto post_callback = std::move(_post_callback);
		if (post_callback != nullptr)
		{
			if (_connection_event_fired)
			{
				post_callback->OnClosed();
			}
		}

		return result;
	}

	Socket::DispatchResult Socket::DispatchEvents()
	{
		switch (_blocking_mode)
		{
			case BlockingMode::Blocking:
#if DEBUG
			{
				std::lock_guard lock_guard(_dispatch_queue_lock);
				[[maybe_unused]] auto count = _dispatch_queue.size();
				OV_ASSERT2(count == 0);
			}
#endif	// DEBUG
				return DispatchResult::Dispatched;

			case BlockingMode::NonBlocking: {
				// Due to the connection callback point, the DispatchEventsInternal() specifically performs mutex.lock inside.
				// std::lock_guard lock_guard(_dispatch_queue_lock);
				return DispatchEventsInternal();
			}
		}

		return DispatchResult::Error;
	}

	ssize_t Socket::SendInternal(const std::shared_ptr<const Data> &data)
	{
		if (GetState() == SocketState::Closed)
		{
			return -1L;
		}

		auto data_to_send = data->GetDataAs<uint8_t>();
		size_t remained = data->GetLength();
		size_t total_sent = 0L;
		static size_t ttt = 0L;

		logap("Trying to send data %zu bytes...", remained);

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				while ((remained > 0L) && (_force_stop == false))
				{
					ssize_t sent = ::send(GetNativeHandle(), data_to_send, remained, MSG_NOSIGNAL | MSG_DONTWAIT);

					if (sent < 0L)
					{
						auto error = Error::CreateErrorFromErrno();

						switch (error->GetCode())
						{
							// Errors that can occur under normal circumstances do not output
							case EAGAIN:
								// Socket buffer is full - retry later
								STATS_COUNTER_INCREASE_RETRY();
								return total_sent;

							case EBADF:
								// Socket is closed somewhere in OME
								break;

							case EPIPE:
								// Broken pipe - maybe peer is disconnected
								break;

							case ECONNRESET:
								// Connection reset - maybe peer is disconnected
								break;

							default:
								logaw("Could not send data: %zd (%s), %s", sent, error->What(), ToString().CStr());
								break;
						}

						STATS_COUNTER_INCREASE_ERROR();

						return sent;
					}

					OV_ASSERT2(static_cast<ssize_t>(remained) >= sent);

					STATS_COUNTER_INCREASE_PPS();

					remained -= sent;
					total_sent += sent;
					data_to_send += sent;

					ttt += sent;

					UpdateLastSentTime();
				}

				break;

			case SocketType::Srt: {
				SRT_MSGCTRL msgctrl{};

				// 10b == start of frame
				//msgctrl.boundary = 2;

				while (remained > 0L)
				{
					// SRT limits packet size up to 1316
					auto to_send = std::min(1316UL, remained);

					if (remained == to_send)
					{
						// 01b == end of frame
						//msgctrl.boundary |= 1;
					}

					int sent = ::srt_sendmsg2(GetNativeHandle(), reinterpret_cast<const char *>(data_to_send), to_send, &msgctrl);

					if (sent == SRT_ERROR)
					{
						auto error = SrtError::CreateErrorFromSrt();

						if (error->GetCode() == SRT_EASYNCSND)
						{
							// Socket buffer is full - retry later
							STATS_COUNTER_INCREASE_RETRY();
							return total_sent;
						}

						STATS_COUNTER_INCREASE_ERROR();
						logaw("Could not send data: %zd (%s)", sent, SrtError::CreateErrorFromSrt()->What());
						return sent;
					}

					OV_ASSERT2(static_cast<ssize_t>(remained) >= sent);

					STATS_COUNTER_INCREASE_PPS();

					remained -= sent;
					total_sent += sent;
					data_to_send += sent;

					msgctrl.boundary = 0;

					UpdateLastSentTime();
				}

				break;
			}

			case SocketType::Unknown:
				logac("Could not send data - unknown socket type");
				OV_ASSERT2(false);
				total_sent = -1L;
				break;
		}

		logap("%zu bytes sent", total_sent);
		return total_sent;
	}

	ssize_t Socket::SendToInternal(const SocketAddress &address, const std::shared_ptr<const Data> &data)
	{
		OV_ASSERT2(address.AddressForIPv4()->sin_addr.s_addr != 0);

		if (GetState() == SocketState::Closed)
		{
			return -1L;
		}

		auto data_to_send = data->GetDataAs<uint8_t>();
		size_t remained = data->GetLength();
		size_t total_sent = 0L;

		logap("Trying to send data %zu bytes to %s...", remained, address.ToString(false).CStr());

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				while ((remained > 0L) && (_force_stop == false))
				{
					ssize_t sent = ::sendto(GetNativeHandle(), data_to_send, remained, MSG_NOSIGNAL | MSG_DONTWAIT, address.Address(), address.AddressLength());

					if (sent < 0L)
					{
						auto error = Error::CreateErrorFromErrno();

						switch (error->GetCode())
						{
							case EAGAIN:
								// Socket buffer is full - retry later
								STATS_COUNTER_INCREASE_RETRY();
								return total_sent;

							case EBADF:
								// Socket is closed somewhere in OME
								break;

							case EPIPE:
								// Broken pipe - maybe peer is disconnected
								break;

							default:
								logaw("Could not send data: %zd (%s)", sent, error->What());
								break;
						}

						STATS_COUNTER_INCREASE_ERROR();
						return sent;
					}

					OV_ASSERT2(static_cast<ssize_t>(remained) >= sent);

					STATS_COUNTER_INCREASE_PPS();

					remained -= sent;
					total_sent += sent;
					data_to_send += sent;
					UpdateLastSentTime();
				}
				break;

			case SocketType::Srt:
				// Does not support SendTo() for SRT
				OV_ASSERT2(false);
				total_sent = -1L;
				break;

			case SocketType::Unknown:
				logac("Could not send data - unknown socket type");
				OV_ASSERT2(false);
				total_sent = -1L;
				break;
		}

		logap("%zu bytes sent", total_sent);

		return total_sent;
	}

	PostProcessMethod Socket::OnDataWritableEvent()
	{
		switch (DispatchEvents())
		{
			case DispatchResult::Dispatched:
				return PostProcessMethod::Nothing;

			case DispatchResult::PartialDispatched:
				return PostProcessMethod::GarbageCollection;

			case DispatchResult::Error:
				return PostProcessMethod::Error;
		}

		return PostProcessMethod::Error;
	}

	void Socket::OnDataAvailableEvent()
	{
		logad("Socket is ready to read");

		if (_callback != nullptr)
		{
			_callback->OnReadable();
		}
	}

	bool Socket::Send(const std::shared_ptr<const Data> &data)
	{
		switch (GetState())
		{
			// When data transfer is requested after disconnection by a worker, etc., it enters here
			case SocketState::Closed:
				[[fallthrough]];
			case SocketState::Disconnected:
				[[fallthrough]];
			case SocketState::Error:
				return false;

			default:
				break;
		}

		if (data == nullptr)
		{
			OV_ASSERT2(data != nullptr);
			return false;
		}

		switch (_blocking_mode)
		{
			case BlockingMode::Blocking:
				return (SendInternal(data) == static_cast<ssize_t>(data->GetLength()));

			case BlockingMode::NonBlocking:
				if (GetType() != SocketType::Udp)
				{
					CHECK_STATE(== SocketState::Connected, false);

					if (AppendCommand({data->Clone()}))
					{
						// Need to send later
						switch (DispatchEvents())
						{
							case DispatchResult::Dispatched:
								break;

							case DispatchResult::PartialDispatched:
								_worker->EnqueueToDispatchLater(GetSharedPtr());
								break;

							case DispatchResult::Error:
								return false;
						}

						return true;
					}

					return false;
				}
				else
				{
					CHECK_STATE2(== SocketState::Created, == SocketState::Bound, false);

					// We don't have to be accurate here, because we'll acquire lock of _dispatch_queue_lock in DispatchEvents()
					if (_dispatch_queue.empty() == false)
					{
						// Send remaining data
						if (DispatchEvents() == DispatchResult::Error)
						{
							return false;
						}
					}

					// Send the data directly
					auto sent = SendInternal(data);

					if (sent == static_cast<ssize_t>(data->GetLength()))
					{
						// The data has been sent
						return true;
					}
					else if (sent == 0L)
					{
						// Need to send later
						return AppendCommand({data->Clone()});
					}

					// An error occurred
					return false;
				}
		}

		return false;
	}

	bool Socket::Send(const void *data, size_t length)
	{
		return Send((data == nullptr) ? nullptr : std::make_shared<Data>(data, length));
	}

	bool Socket::SendTo(const SocketAddress &address, const std::shared_ptr<const Data> &data)
	{
		switch (GetState())
		{
			// When data transfer is requested after disconnection by a worker, etc., it enters here
			case SocketState::Closed:
				[[fallthrough]];
			case SocketState::Disconnected:
				[[fallthrough]];
			case SocketState::Error:
				return false;

			default:
				break;
		}

		if (data == nullptr)
		{
			OV_ASSERT2(data != nullptr);
			return false;
		}

		switch (_blocking_mode)
		{
			case BlockingMode::Blocking:
				return (SendToInternal(address, data) == static_cast<ssize_t>(data->GetLength()));

			case BlockingMode::NonBlocking:
				if (GetType() != SocketType::Udp)
				{
					CHECK_STATE(== SocketState::Connected, false);

					if (AppendCommand({data->Clone()}))
					{
						// Need to send later
						switch (DispatchEvents())
						{
							case DispatchResult::Dispatched:
								break;

							case DispatchResult::PartialDispatched:
								_worker->EnqueueToDispatchLater(GetSharedPtr());
								break;

							case DispatchResult::Error:
								return false;
						}

						return true;
					}

					return false;
				}
				else
				{
					CHECK_STATE2(== SocketState::Created, == SocketState::Bound, false);

					// We don't have to be accurate here, because we'll acquire lock of _dispatch_queue_lock in DispatchEvents()
					if (_dispatch_queue.empty() == false)
					{
						// Send remaining data
						if (DispatchEvents() == DispatchResult::Error)
						{
							return false;
						}
					}

					// Send the data directly
					auto sent = SendToInternal(address, data);

					if (sent == static_cast<ssize_t>(data->GetLength()))
					{
						// The data has been sent
						return true;
					}
					else if (sent == 0L)
					{
						// Need to send later
						return AppendCommand({address, data->Clone()});
					}
					else
					{
						// An error occurred
					}

					return false;
				}
		}

		return false;
	}

	bool Socket::SendTo(const SocketAddress &address, const void *data, size_t length)
	{
		return SendTo(address, (data == nullptr) ? nullptr : std::make_shared<Data>(data, length));
	}

	std::shared_ptr<const SocketError> Socket::Recv(std::shared_ptr<Data> &data, bool non_block)
	{
		OV_ASSERT2(data != nullptr);
		OV_ASSERT(data->GetCapacity() > 0, "Must specify a data size in advance using Reserve().");

		size_t read_bytes;

		data->SetLength(data->GetCapacity());

		auto error = Recv(data->GetWritableData(), data->GetCapacity(), &read_bytes, non_block);

		if (error != nullptr)
		{
			return error;
		}

		data->SetLength(read_bytes);

		return nullptr;
	}

	std::shared_ptr<const SocketError> Socket::Recv(void *data, size_t length, size_t *received_length, bool non_block)
	{
		if (GetState() == SocketState::Closed)
		{
			return SocketError::CreateError("Socket is closed");
		}

		OV_ASSERT2(data != nullptr);
		OV_ASSERT2(received_length != nullptr);

		logap("Trying to read from the socket...");

		ssize_t read_bytes = -1;
		std::shared_ptr<SocketError> socket_error;

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				read_bytes = ::recv(GetNativeHandle(), data, length,
									((_blocking_mode == BlockingMode::NonBlocking) || non_block) ? MSG_DONTWAIT : 0);

				if (read_bytes <= 0L)
				{
					auto error = Error::CreateErrorFromErrno();

					if (error->GetCode() == EAGAIN)
					{
						if (_blocking_mode == BlockingMode::NonBlocking)
						{
							// Timed out
							read_bytes = 0L;
							// Actually, it is not an error
							socket_error = nullptr;
						}
						else
						{
							socket_error = SocketError::CreateError(error->GetCode(), "Receive timed out: %s", error->GetMessage().CStr());
						}
					}
					else
					{
						socket_error = SocketError::CreateError(error);
					}
				}

				break;

			case SocketType::Srt: {
				SRT_MSGCTRL msg_ctrl{};
				read_bytes = ::srt_recvmsg2(GetNativeHandle(), reinterpret_cast<char *>(data), static_cast<int>(length), &msg_ctrl);

				if (read_bytes <= 0L)
				{
					auto error = SrtError::CreateErrorFromSrt();

					if (error->GetCode() == SRT_EASYNCRCV)
					{
						// Timed out
						read_bytes = 0L;
						// Actually, it is not an error
						socket_error = nullptr;
					}
					else
					{
						socket_error = SocketError::CreateError(error->GetCode(), "Receive timed out (SRT): %s", error->GetMessage().CStr());
					}
				}

				break;
			}

			default:
				break;
		}

		if (socket_error != nullptr)
		{
			if (
				((_socket.GetType() != SocketType::Srt) && (read_bytes == 0L)) ||
				((_socket.GetType() == SocketType::Srt) && (socket_error->GetCode() == SRT_ECONNLOST)))
			{
				socket_error = SocketError::CreateError("Remote is disconnected");
				*received_length = 0UL;

				CloseWithState(SocketState::Disconnected);

				return socket_error;
			}
			else if (read_bytes < 0L)
			{
				if (_socket.GetType() != SocketType::Srt)
				{
					SocketState new_state = SocketState::Error;

					switch (socket_error->GetCode())
					{
						// Errors that can occur under normal circumstances do not output
						case EBADF:
							// Socket is closed somewhere in OME
							break;

						case ECONNRESET:
							// Peer is disconnected
							new_state = SocketState::Disconnected;
							break;

						case ENOTCONN:
							// Transport endpoint is not connected
							break;

						case EAGAIN:
							// Timed out
							OV_ASSERT2(_blocking_mode == BlockingMode::Blocking);
							break;

						default:
							logae("An error occurred while read data: %s\nStack trace: %s",
								  socket_error->What(),
								  StackTrace::GetStackTrace().CStr());
					}

					CloseWithState(new_state);
				}
				else
				{
					switch (socket_error->GetCode())
					{
						default:
							logae("An error occurred while read data: %s\nStack trace: %s",
								  socket_error->What(),
								  StackTrace::GetStackTrace().CStr());
					}

					CloseWithState(SocketState::Error);
				}
			}
		}
		else
		{
			logap("%zd bytes read", read_bytes);
			*received_length = static_cast<size_t>(read_bytes);
			UpdateLastRecvTime();
		}

		return socket_error;
	}

	std::shared_ptr<const SocketError> Socket::RecvFrom(std::shared_ptr<Data> &data, SocketAddress *address, bool non_block)
	{
		OV_ASSERT2(_socket.IsValid());
		OV_ASSERT2(data != nullptr);
		OV_ASSERT2(data->GetCapacity() > 0);

		std::shared_ptr<SocketError> socket_error;

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp: {
				sockaddr_in remote = {0};
				socklen_t remote_length = sizeof(remote);

				logad("Trying to read from the socket...");
				data->SetLength(data->GetCapacity());

				ssize_t read_bytes = ::recvfrom(GetNativeHandle(), data->GetWritableData(), data->GetLength(),
												((_blocking_mode == BlockingMode::NonBlocking) || non_block) ? MSG_DONTWAIT : 0,
												reinterpret_cast<sockaddr *>(&remote), &remote_length);

				if (read_bytes < 0L)
				{
					auto error = Error::CreateErrorFromErrno();

					data->SetLength(0L);

					if (error->GetCode() == EAGAIN)
					{
						// Timed out
						socket_error = nullptr;
					}
					else
					{
						socket_error = SocketError::CreateError(error);
					}
				}
				else
				{
					logad("%zd bytes read", read_bytes);

					data->SetLength(read_bytes);

					if (address != nullptr)
					{
						*address = SocketAddress(remote);
					}

					UpdateLastRecvTime();
				}
				break;
			}

			case SocketType::Srt:
				// Does not support RecvFrom() for SRT
				OV_ASSERT2(false);
				socket_error = SocketError::CreateError("RecvFrom() is not supported operation while using SRT");
				break;

			case SocketType::Unknown:
				OV_ASSERT2(false);
				socket_error = SocketError::CreateError("Unknown socket type");
				break;
		}

		if (socket_error != nullptr)
		{
			logae("An error occurred while read data: %s\nStack trace: %s",
				  socket_error->What(),
				  StackTrace::GetStackTrace().CStr());

			CloseWithState(SocketState::Error);
		}

		return socket_error;
	}

	std::chrono::system_clock::time_point Socket::GetLastRecvTime() const
	{
		return _last_recv_time;
	}

	std::chrono::system_clock::time_point Socket::GetLastSentTime() const
	{
		return _last_sent_time;
	}

	void Socket::UpdateLastRecvTime()
	{
		_last_recv_time = std::chrono::high_resolution_clock::now();
	}
	
	void Socket::UpdateLastSentTime()
	{
		_last_sent_time = std::chrono::high_resolution_clock::now();
	}

	bool Socket::Flush()
	{
		CHECK_STATE(== SocketState::Connected, false);

		// Dispatch ALL commands
		while (_dispatch_queue.size() > 0)
		{
			if (DispatchEvents() == DispatchResult::Error)
			{
				return false;
			}
		}

		return true;
	}

	bool Socket::CloseIfNeeded()
	{
		if (_blocking_mode == BlockingMode::Blocking)
		{
			return Close();
		}

		return IsClosing() || Close();
	}

	bool Socket::CloseWithState(SocketState new_state)
	{
		CHECK_STATE(>= SocketState::Closed, false);

		logad("Closing %s...", GetRemoteAddress() != nullptr ? GetRemoteAddress()->ToString(false).CStr() : GetStreamId().CStr());

		if (GetState() == SocketState::Closed)
		{
			// Suppress error message
			return false;
		}

		switch (_blocking_mode)
		{
			case BlockingMode::Blocking: {
				bool result = true;

				if (GetState() != SocketState::Error)
				{
					if (HalfClose() == DispatchResult::Error)
					{
						result = false;
					}
					if (WaitForHalfClose() == DispatchResult::Error)
					{
						result = false;
					}
				}

				// Close regardless of result
				{
					std::lock_guard lock_guard(_dispatch_queue_lock);

					if (CloseInternal())
					{
						SetState(new_state);
					}
					else
					{
						result = false;
					}
				}

				return result;
			}

			case BlockingMode::NonBlocking: {
				SOCKET_PROFILER_INIT();
				std::lock_guard lock_guard(_dispatch_queue_lock);
				SOCKET_PROFILER_AFTER_LOCK();

				SOCKET_PROFILER_POST_HANDLER([&](int64_t lock_elapsed, int64_t total_elapsed) {
					if ((lock_elapsed > 100) || (_dispatch_queue.size() > 10))
					{
						logtw("[SockProfiler] Close() - %s, Queue: %zu, Lock: %dms, Total: %dms", ToString().CStr(), _dispatch_queue.size(), lock_elapsed, total_elapsed);
					}
				});

				if (_has_close_command == false)
				{
					logad("Enqueuing close command (new_state: %s)", StringFromSocketState(new_state));

					_has_close_command = true;

					if ((GetState() != SocketState::Disconnected) && (GetState() != SocketState::Error))
					{
						_dispatch_queue.emplace_back(DispatchCommand::Type::HalfClose);
						_dispatch_queue.emplace_back(DispatchCommand::Type::WaitForHalfClose);
					}

					_dispatch_queue.emplace_back(DispatchCommand::Type::Close, new_state);
				}
				else
				{
					logad("This socket already has close command (Do not need to call Close*() in this case)\n%s", StackTrace::GetStackTrace().CStr());
				}

				_worker->EnqueueToDispatchLater(GetSharedPtr());

				return true;
			}
		}

		return false;
	}

	bool Socket::Close()
	{
		return CloseWithState(SocketState::Closed);
	}

	bool Socket::CloseImmediately()
	{
		std::lock_guard lock_guard(_dispatch_queue_lock);

		// We must call DispatchEvents() so that the _post_callback can be called.
		return CloseInternal() && (DispatchEvents() != DispatchResult::Error);
	}

	bool Socket::CloseImmediatelyWithState(SocketState new_state)
	{
		std::lock_guard lock_guard(_dispatch_queue_lock);

		// We must call DispatchEvents() so that the _post_callback can be called.
		if (CloseInternal() && (DispatchEvents() != DispatchResult::Error))
		{
			SetState(new_state);
			return true;
		}

		return false;
	}

	Socket::DispatchResult Socket::HalfClose()
	{
		if (GetType() == SocketType::Tcp)
		{
			if (_socket.IsValid())
			{
				// Send FIN
				::shutdown(GetNativeHandle(), SHUT_WR);
			}
		}

		return DispatchResult::Dispatched;
	}

	Socket::DispatchResult Socket::WaitForHalfClose()
	{
		if (GetType() != SocketType::Tcp)
		{
			return DispatchResult::Dispatched;
		}

		switch (GetState())
		{
			case SocketState::Created:
				[[fallthrough]];
			case SocketState::Disconnected:
				return DispatchResult::Dispatched;

			default:
				break;
		}

		while (true)
		{
			char buffer[MAX_BUFFER_SIZE];
			int result = ::recv(GetNativeHandle(), buffer, MAX_BUFFER_SIZE, MSG_DONTWAIT);

			if (result < 0L)
			{
				auto error = Error::CreateErrorFromErrno();

				switch (error->GetCode())
				{
					case EBADF:
						// Suppress "Bad file descriptor" message
						break;

					case EAGAIN:
						// Peer doesn't send ACK/FIN yet - ignores this
						return DispatchResult::Dispatched;

					case ECONNRESET:
						// Suppress "Connection reset by peer" message
						break;

					case ENOTCONN:
						// Suppress "Transport endpoint is not connected" message
						break;

					default:
						logae("An error occurred while half-closing: %s", error->What());
						break;
				}

				return DispatchResult::Error;
			}
			else if (result == 0)
			{
				logad("Half closed");
				return DispatchResult::Dispatched;
			}
			else
			{
				// Ignores those data
			}
		}
	}

	bool Socket::CloseInternal()
	{
		CHECK_STATE(!= SocketState::Closed, false);

		_post_callback = std::move(_callback);

		if (_socket.IsValid())
		{
			switch (GetType())
			{
				case SocketType::Tcp:
					::close(GetNativeHandle());
					break;

				case SocketType::Udp:
					::close(GetNativeHandle());
					break;

				case SocketType::Srt:
					::srt_close(GetNativeHandle());
					break;

				default:
					break;
			}

			_socket.SetValid(false);

			if (_dispatch_queue.size() > 0)
			{
				if (_dispatch_queue.front().IsCloseCommand() == false)
				{
					logaw("Socket was closed even though there was %zu commands in the socket", _dispatch_queue.size());

#if DEBUG
					for (auto &queue : _dispatch_queue)
					{
						logad("  - Command: %s", queue.ToString().CStr());
					}
#endif	// DEBUG
				}
			}

			_dispatch_queue.clear();

			logad("Socket is closed successfully");

			return true;
		}

		logad("Socket is already closed");
		OV_ASSERT(((_state == SocketState::Closed) ||
				   (_state == SocketState::Disconnected) ||
				   (_state == SocketState::Error)),
				  "Invalid state: %s", StringFromSocketState(_state));

		return false;
	}

	String Socket::ToString(const char *class_name) const
	{
		ov::String caller(class_name);
		bool ignore_privacy_protection = true;

		// ClientSocket must follow privacy rule
		if (caller == "ClientSocket")
		{
			ignore_privacy_protection = false;
		}

		return String::FormatString(
			"<%s: %p, #%d, %s, %s, %s%s%s>",
			class_name, this,
			GetNativeHandle(), StringFromSocketState(_state),
			StringFromSocketType(GetType()),
			StringFromBlockingMode(_blocking_mode),
			(_remote_address != nullptr) ? ", " : "",
			(_remote_address != nullptr) ? _remote_address->ToString(ignore_privacy_protection).CStr() : "");
	}

	String Socket::ToString() const
	{
		return ToString("Socket");
	}
}  // namespace ov
