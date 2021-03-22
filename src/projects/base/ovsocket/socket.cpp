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

		ov::StopWatch sw;
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

	Socket::Socket(PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker)
		: _worker(worker)
	{
	}

	// Creates a socket using remote information.
	// Remote information is present, considered already connected
	Socket::Socket(PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker,
				   SocketWrapper socket, const SocketAddress &remote_address)
		: Socket(token, worker)
	{
		_socket = socket;
		_state = SocketState::Connected;

		// 로컬 정보는 없으며, 상대 정보만 있음. 즉, send만 가능
		_remote_address = std::make_shared<SocketAddress>(remote_address);
	}

	Socket::~Socket()
	{
		// Verify that the socket is closed normally
		CHECK_STATE(== SocketState::Closed, );
		OV_ASSERT(_socket.IsValid() == false, "Socket is not closed. Current state: %s", StringFromSocketState(GetState()));
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

		CloseInternal();

		return false;
	}

	bool Socket::SetBlockingInternal(bool blocking)
	{
		int result;

		if (_socket.IsValid() == false)
		{
			logae("Could not make %sblocking socket (Invalid socket)", blocking ? "" : "non ");
			OV_ASSERT2(_socket.IsValid());
			return false;
		}

		switch (GetType())
		{
			case SocketType::Tcp:
			case SocketType::Udp: {
				result = ::fcntl(GetNativeHandle(), F_GETFL, 0);

				if (result == -1)
				{
					logae("Could not obtain flags from socket %d (%d)", GetNativeHandle(), result);
					return false;
				}

				int flags = result;

				if (blocking)
				{
					flags &= ~O_NONBLOCK;
				}
				else
				{
					flags |= O_NONBLOCK;
				}

				result = ::fcntl(GetNativeHandle(), F_SETFL, flags);

				if (result == -1)
				{
					logae("Could not set flags to socket %d: %s", GetNativeHandle(), ov::Error::CreateErrorFromErrno()->ToString().CStr());
					return false;
				}

				_is_nonblock = !blocking;

				return true;
			}

			case SocketType::Srt:
				if (SetSockOpt(SRTO_RCVSYN, blocking) && SetSockOpt(SRTO_SNDSYN, blocking))
				{
					_is_nonblock = !blocking;
					return true;
				}

				logae("Could not set flags to SRT socket %d: %s", GetNativeHandle(), ov::Error::CreateErrorFromSrt()->ToString().CStr());
				return false;

			default:
				OV_ASSERT(false, "Invalid socket type: %d", GetType());
				break;
		}

		return false;
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

	bool Socket::AttachToWorker()
	{
		return _worker->AttachToWorker(GetSharedPtr());
	}

	bool Socket::MakeBlocking()
	{
		return SetBlockingInternal(true);
	}

	bool Socket::MakeNonBlocking(std::shared_ptr<SocketAsyncInterface> callback)
	{
		if (SetBlockingInternal(false))
		{
			_callback = std::move(callback);
			return true;
		}

		_callback = nullptr;
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

				logae("Could not listen: %s", Error::CreateErrorFromErrno()->ToString().CStr());
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

	std::shared_ptr<ov::Error> Socket::Connect(const SocketAddress &endpoint, int timeout_msec)
	{
		OV_ASSERT2(_socket.IsValid());
		CHECK_STATE(== SocketState::Created, ov::Error::CreateError(EINVAL, "Invalid state: %d", static_cast<int>(_state)));

		std::shared_ptr<ov::Error> error;

		// To set connection timeout
		bool origin_nonblock_flag = _is_nonblock;

		switch (GetType())
		{
			case SocketType::Tcp:
			case SocketType::Udp: {
				if (timeout_msec > 0 && timeout_msec < Infinite)
				{
					if (origin_nonblock_flag == false)
					{
						MakeNonBlocking(nullptr);
					}
				}

				int result = ::connect(GetNativeHandle(), endpoint.Address(), endpoint.AddressLength());

				if (result == 0)
				{
					if (origin_nonblock_flag == false)
					{
						MakeBlocking();
					}

					SetState(SocketState::Connected);
					return nullptr;
				}
				else if (result < 0)
				{
					// Check timeout
					if (errno == EINPROGRESS)
					{
						do
						{
							// For timeout and fastest to notice connection success
							struct epoll_event triggered_events;
							int ep_fd = epoll_create1(0);
							struct epoll_event ep_event;
							ep_event.events = EPOLLOUT | EPOLLIN | EPOLLERR;
							ep_event.data.fd = GetSocket().GetNativeHandle();

							if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, GetSocket().GetNativeHandle(), &ep_event) == -1)
							{
								close(ep_fd);
								break;
							}

							auto num_events = epoll_wait(ep_fd, &triggered_events, 1, timeout_msec);

							// timeout or error
							if (num_events <= 0)
							{
								close(ep_fd);
								break;
							}

							// Socket selected for write
							int error_value;

							if (GetSockOpt(SO_ERROR, &error_value) == false)
							{
								close(ep_fd);
								break;
							}

							// Not connected
							if (error_value != 0)
							{
								close(ep_fd);
								break;
							}

							// Recover origin state
							if (origin_nonblock_flag == false)
							{
								MakeBlocking();
							}

							SetState(SocketState::Connected);
							close(ep_fd);

							return nullptr;

						} while (true);
					}
				}

				error = ov::Error::CreateErrorFromErrno();
				break;
			}

			case SocketType::Srt:
				if (SetSockOpt(SRTO_CONNTIMEO, timeout_msec))
				{
					if (::srt_connect(GetNativeHandle(), endpoint.Address(), endpoint.AddressLength()) != SRT_ERROR)
					{
						SetState(SocketState::Connected);
						return nullptr;
					}
				}

				error = ov::Error::CreateErrorFromSrt();

				break;

			default:
				error = ov::Error::CreateError("Socket", "Not implemented");
				break;
		}

		return error;
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

	std::shared_ptr<ov::SocketAddress> Socket::GetLocalAddress() const
	{
		return _local_address;
	}

	std::shared_ptr<ov::SocketAddress> Socket::GetRemoteAddress() const
	{
		return _remote_address;
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
			auto error = ov::Error::CreateErrorFromSrt();
			logaw("Could not set option: %d (result: %s)", option, error->ToString().CStr());
			return false;
		}

		return true;
	}

	SocketState Socket::GetState() const
	{
		return _state;
	}

	void Socket::SetState(SocketState state)
	{
		_state = state;
	}

	SocketType Socket::GetType() const
	{
		return _socket.GetType();
	}

	Socket::DispatchResult Socket::DispatchInternal(DispatchCommand &command)
	{
		SOCKET_PROFILER_INIT();
		SOCKET_PROFILER_POST_HANDLER([&](int64_t lock_elapsed, int64_t total_elapsed) {
			if (total_elapsed > 100)
			{
				logtw("[SockProfiler] DispatchInternal() - %s, Total: %dms", ToString().CStr(), total_elapsed);
			}
		});

		ssize_t sent_bytes;
		auto &data = command.data;

		logap("Dispatching event: %s", command.ToString().CStr());

		switch (command.type)
		{
			case DispatchCommand::Type::Connected:
				if (_callback != nullptr)
				{
					_connection_event_fired = true;
					_callback->OnConnected();
				}
				return DispatchResult::Dispatched;

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

			case DispatchCommand::Type::Close:
				logad("Trying to close the socket...");

				// Remove the socket from epoll
				if (_worker->ReleaseSocket(this->GetSharedPtr()))
				{
					// CloseInternal() will be called in ReleaseSocket()
					return DispatchResult::Dispatched;
				}

				logae("Could not release socket from worker");
				CloseInternal();

				OV_ASSERT2(false);
				return DispatchResult::Error;
		}

		if (sent_bytes == static_cast<ssize_t>(command.data->GetLength()))
		{
			return DispatchResult::Dispatched;
		}

		// Since some data has been sent, the time needs to be updated.
		command.UpdateTime();
		data = data->Subdata(sent_bytes);

		logad("Some data has not been sent: %ld bytes left", data->GetLength());

		return DispatchResult::PartialDispatched;
	}

	Socket::DispatchResult Socket::DispatchEvents()
	{
		SOCKET_PROFILER_INIT();

		DispatchResult result = DispatchResult::Dispatched;

		{
			std::lock_guard lock_guard(_dispatch_queue_lock);
			SOCKET_PROFILER_AFTER_LOCK();

			[[maybe_unused]] auto count = _dispatch_queue.size();
			SOCKET_PROFILER_POST_HANDLER([&](int64_t lock_elapsed, int64_t total_elapsed) {
				if ((lock_elapsed > 100) || (count > 10) || (_dispatch_queue.size() > 10))
				{
					logtw("[SockProfiler] DispatchEvents() - %s, Before Queue: %zu, After Queue: %zu, Lock: %dms, Total: %dms", ToString().CStr(), count, _dispatch_queue.size(), lock_elapsed, total_elapsed);
				}
			});

			if (_dispatch_queue.empty())
			{
				return DispatchResult::Dispatched;
			}

			logap("Dispatching events (count: %zu)...", _dispatch_queue.size());

			while (_dispatch_queue.empty() == false)
			{
				auto &front = _dispatch_queue.front();

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

				result = DispatchInternal(front);

				if (result == DispatchResult::Dispatched)
				{
					if (_dispatch_queue.size() > 0)
					{
						// Dispatches the next item
						_dispatch_queue.pop_front();
					}
					else
					{
						// All items are dispatched int DispatchInternal();
					}

					continue;
				}
				else if (result == DispatchResult::PartialDispatched)
				{
					// The data is not fully processed and will not be removed from queue

					// Close-related commands will be processed when we receive the event from epoll later
				}
				else
				{
					// An error occurred

					if (is_close_command)
					{
						// Ignore errors that occurred during close
						result = DispatchResult::Dispatched;
						break;
					}
				}

				break;
			}
		}

		// Since the resource is usually cleaned inside the OnClosed() callback,
		// callback is performed outside the lock_guard to prevent acquiring the lock.
		if (_post_callback != nullptr)
		{
			if (_connection_event_fired)
			{
				_post_callback->OnClosed();
			}
		}

		return result;
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
								logaw("Could not send data: %zd (%s)", sent, error->ToString().CStr());
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
						auto error = Error::CreateErrorFromSrt();

						if (error->GetCode() == SRT_EASYNCSND)
						{
							// Socket buffer is full - retry later
							STATS_COUNTER_INCREASE_RETRY();
							return total_sent;
						}

						STATS_COUNTER_INCREASE_ERROR();
						logaw("Could not send data: %zd (%s)", sent, Error::CreateErrorFromSrt()->ToString().CStr());
						return sent;
					}

					OV_ASSERT2(static_cast<ssize_t>(remained) >= sent);

					STATS_COUNTER_INCREASE_PPS();

					remained -= sent;
					total_sent += sent;
					data_to_send += sent;

					msgctrl.boundary = 0;
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

		logap("Trying to send data %zu bytes to %s...", remained, address.ToString().CStr());

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
								logaw("Could not send data: %zd (%s)", sent, error->ToString().CStr());
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

		if (GetType() != SocketType::Udp)
		{
			CHECK_STATE(== SocketState::Connected, false);

			if (AppendCommand({data->Clone()}) == false)
			{
				return false;
			}

			return (DispatchEvents() != DispatchResult::Error);
		}
		else
		{
			CHECK_STATE2(== SocketState::Created, == SocketState::Bound, false);

			// We don't have to be accurate here, because we'll acquire lock of _dispatch_queue_lock in DispatchEvent()
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
			else
			{
				// An error occurred
				return false;
			}
		}
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

		if (GetType() != SocketType::Udp)
		{
			CHECK_STATE(== SocketState::Connected, false);

			if (AppendCommand({address, data->Clone()}) == false)
			{
				return false;
			}

			return (DispatchEvents() != DispatchResult::Error);
		}
		else
		{
			CHECK_STATE2(== SocketState::Created, == SocketState::Bound, false);

			// We don't have to be accurate here, because we'll acquire lock of _dispatch_queue_lock in DispatchEvent()
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
				return false;
			}
		}
	}

	bool Socket::SendTo(const SocketAddress &address, const void *data, size_t length)
	{
		return SendTo(address, (data == nullptr) ? nullptr : std::make_shared<Data>(data, length));
	}

	std::shared_ptr<ov::Error> Socket::Recv(std::shared_ptr<Data> &data, bool non_block)
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

	std::shared_ptr<ov::Error> Socket::Recv(void *data, size_t length, size_t *received_length, bool non_block)
	{
		if (GetState() == SocketState::Closed)
		{
			return Error::CreateError("Socket", "Socket is closed");
		}

		OV_ASSERT2(data != nullptr);
		OV_ASSERT2(received_length != nullptr);

		logap("Trying to read from the socket...");

		ssize_t read_bytes = -1;
		std::shared_ptr<Error> error;

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				read_bytes = ::recv(GetNativeHandle(), data, length, (_is_nonblock || non_block) ? MSG_DONTWAIT : 0);

				if (read_bytes <= 0L)
				{
					error = Error::CreateErrorFromErrno();

					if (error->GetCode() == EAGAIN)
					{
						// Timed out
						read_bytes = 0L;
						error = nullptr;
					}
				}

				break;

			case SocketType::Srt: {
				SRT_MSGCTRL msg_ctrl{};
				read_bytes = ::srt_recvmsg2(GetNativeHandle(), reinterpret_cast<char *>(data), static_cast<int>(length), &msg_ctrl);

				if (read_bytes <= 0L)
				{
					error = Error::CreateErrorFromSrt();

					if (error->GetCode() == SRT_EASYNCRCV)
					{
						// Timed out
						read_bytes = 0L;
						error = nullptr;
					}
				}

				break;
			}

			default:
				break;
		}

		if (error != nullptr)
		{
			if (read_bytes == 0L)
			{
				logad("Remote is disconnected: %s", error->ToString().CStr());
				*received_length = 0UL;

				SetState(SocketState::Disconnected);
				Close();

				return error;
			}
			else if (read_bytes < 0L)
			{
				switch (error->GetCode())
				{
					// Errors that can occur under normal circumstances do not output
					case EBADF:
						// Socket is closed somewhere in OME
						break;

					case ECONNRESET:
						// Peer is disconnected
						break;

					case ENOTCONN:
						// Transport endpoint is not connected
						break;

					default:
						logae("An error occurred while read data: %s\nStack trace: %s",
							  error->ToString().CStr(),
							  ov::StackTrace::GetStackTrace().CStr());
				}
				SetState(SocketState::Error);
			}
		}
		else
		{
			logap("%zd bytes read", read_bytes);
			*received_length = static_cast<size_t>(read_bytes);
		}

		return error;
	}

	std::shared_ptr<ov::Error> Socket::RecvFrom(std::shared_ptr<Data> &data, SocketAddress *address, bool non_block)
	{
		OV_ASSERT2(_socket.IsValid());
		OV_ASSERT2(data != nullptr);
		OV_ASSERT2(data->GetCapacity() > 0);

		std::shared_ptr<Error> error;

		switch (GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp: {
				sockaddr_in remote = {0};
				socklen_t remote_length = sizeof(remote);

				logad("Trying to read from the socket...");
				data->SetLength(data->GetCapacity());

				ssize_t read_bytes = ::recvfrom(GetNativeHandle(), data->GetWritableData(), data->GetLength(), (_is_nonblock || non_block) ? MSG_DONTWAIT : 0, (sockaddr *)&remote, &remote_length);

				if (read_bytes < 0L)
				{
					error = Error::CreateErrorFromErrno();

					data->SetLength(0L);

					if (error->GetCode() == EAGAIN)
					{
						// Timed out
						error = nullptr;
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
				}
				break;
			}

			case SocketType::Srt:
				// Does not support RecvFrom() for SRT
				OV_ASSERT2(false);
				error = ov::Error::CreateError("Socket", "RecvFrom() is not supported operation while using SRT");
				break;

			case SocketType::Unknown:
				OV_ASSERT2(false);
				error = ov::Error::CreateError("Socket", "Unknown socket type");
				break;
		}

		if (error != nullptr)
		{
			logae("An error occurred while read data: %s\nStack trace: %s",
				  error->ToString().CStr(),
				  ov::StackTrace::GetStackTrace().CStr());

			SetState(SocketState::Error);
		}

		return error;
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
		if (IsClosing() == false)
		{
			return Close();
		}

		// Socket is already closed
		return false;
	}

	bool Socket::Close()
	{
		CHECK_STATE(>= SocketState::Closed, false);

		if (GetState() == SocketState::Error)
		{
			// Suppress error message
			return false;
		}

		{
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
				logad("Enqueuing close command");
				_has_close_command = true;

				if (GetState() != SocketState::Disconnected)
				{
					_dispatch_queue.emplace_back(DispatchCommand::Type::HalfClose);
					_dispatch_queue.emplace_back(DispatchCommand::Type::WaitForHalfClose);
				}

				_dispatch_queue.emplace_back(DispatchCommand::Type::Close);
			}
			else
			{
				logad("This socket already has close command (Do not need to call Close*() in this case)");
			}
		}

		return DispatchEvents() != DispatchResult::Error;
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

		if (GetState() == ov::SocketState::Created)
		{
			return Socket::DispatchResult::Dispatched;
		}

		while (true)
		{
			char buffer[MAX_BUFFER_SIZE];
			int result = ::recv(GetNativeHandle(), buffer, MAX_BUFFER_SIZE, MSG_DONTWAIT);

			if (result < 0L)
			{
				auto error = ov::Error::CreateErrorFromErrno();

				if (error->GetCode() == EAGAIN)
				{
					// Peer doesn't send ACK/FIN yet - ignores this
					return DispatchResult::Dispatched;
				}

				logae("An error occurred while half-closing: %s", error->ToString().CStr());
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

			logad("Socket is closed successfully");
			SetState(SocketState::Closed);

			return true;
		}

		logad("Socket is already closed");
		OV_ASSERT2(_state == SocketState::Closed);

		return false;
	}

	String Socket::ToString(const char *class_name) const
	{
		return String::FormatString(
			"<%s: %p, #%d, state: %s, %s, %s>",
			class_name, this,
			GetNativeHandle(), StringFromSocketState(_state),
			StringFromSocketType(GetType()),
			(_remote_address != nullptr) ? _remote_address->ToString().CStr() : "N/A");
	}

	String Socket::ToString() const
	{
		return ToString("Socket");
	}
}  // namespace ov
