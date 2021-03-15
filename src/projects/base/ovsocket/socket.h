//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <netinet/in.h>
#include <netinet/tcp.h>

#include "socket_address.h"
#include "socket_wrapper.h"

#if !IS_MACOS
#	include <sys/epoll.h>
#	include <linux/sockios.h>
#endif	// !IS_MACOS

#include <sys/socket.h>

#include <functional>
#include <map>
#include <memory>
#include <utility>

// Failure to send data for the specified time period will be considered an error.
// For example, it can occur when EAGAIN continues to occur for a period of time, or when the peer's TCP window is full and no longer receives data.
#define OV_SOCKET_EXPIRE_TIMEOUT (10 * 1000)

namespace ov
{
	// Forward declaration
	class Socket;
	class SocketPoolWorker;

	class SocketAsyncInterface
	{
	public:
		virtual void OnConnected() = 0;
		virtual void OnReadable() = 0;
		virtual void OnClosed() = 0;
	};

	class Socket : public EnableSharedFromThis<Socket>
	{
	protected:
		friend class SocketPoolWorker;

		OV_SOCKET_DECLARE_PRIVATE_TOKEN();

		enum class DispatchResult
		{
			// Command is dispatched
			Dispatched,
			// Command is dispatched, and some data is remained
			PartialDispatched,
			// An error occurred
			Error
		};

	public:
		// SocketPoolWorker can only be created within SocketPool
		Socket(PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker);
		Socket(PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker,
			   SocketWrapper socket, const SocketAddress &remote_address);

		// Disable copy & move operator
		Socket(const Socket &socket) = delete;
		Socket(Socket &&socket) = delete;

		~Socket() override;

		std::shared_ptr<SocketPoolWorker> GetSocketPoolWorker()
		{
			return _worker;
		}

		std::shared_ptr<const SocketPoolWorker> GetSocketPoolWorker() const
		{
			return _worker;
		}

		bool AttachToWorker();

		bool MakeBlocking();
		bool MakeNonBlocking(std::shared_ptr<SocketAsyncInterface> callback);

		bool Bind(const SocketAddress &address);
		bool Listen(int backlog = SOMAXCONN);
		SocketWrapper Accept(SocketAddress *client);
		std::shared_ptr<Error> Connect(const SocketAddress &endpoint, int timeout_msec = Infinite);

		bool SetRecvTimeout(const timeval &tv);

		std::shared_ptr<SocketAddress> GetLocalAddress() const;
		std::shared_ptr<SocketAddress> GetRemoteAddress() const;

		// for system socket
		template <class T>
		bool SetSockOpt(int proto, int option, const T &value)
		{
			return SetSockOpt(proto, option, &value, (socklen_t)sizeof(T));
		}

		template <class T>
		bool SetSockOpt(int option, const T &value)
		{
			return SetSockOpt<T>(SOL_SOCKET, option, value);
		}

		bool SetSockOpt(int proto, int option, const void *value, socklen_t value_length);
		bool SetSockOpt(int option, const void *value, socklen_t value_length);

		template <class T>
		bool GetSockOpt(int option, T *value) const
		{
			return GetSockOpt(SOL_SOCKET, option, value, (socklen_t)sizeof(T));
		}

		bool GetSockOpt(int proto, int option, void *value, socklen_t value_length) const;

		// for SRT socket
		template <class T>
		bool SetSockOpt(SRT_SOCKOPT option, const T &value)
		{
			return SetSockOpt(option, &value, static_cast<int>(sizeof(T)));
		}

		bool SetSockOpt(SRT_SOCKOPT option, const void *value, int value_length);

		SocketState GetState() const;

		void SetState(SocketState state);

		SocketWrapper GetSocket() const
		{
			return _socket;
		}

		int GetNativeHandle() const
		{
			return _socket.GetNativeHandle();
		}

		SocketType GetType() const;
		std::shared_ptr<SocketAsyncInterface> GetAsyncInterface()
		{
			return _callback;
		}

		bool Send(const std::shared_ptr<const Data> &data);
		bool Send(const void *data, size_t length);

		bool SendTo(const SocketAddress &address, const std::shared_ptr<const Data> &data);
		bool SendTo(const SocketAddress &address, const void *data, size_t length);

		// When Recv is called in non-blocking mode,
		//
		// 1. return != nullptr: An error occurred (Include disconnecting the client)
		// 2. return == nullptr:
		// 2-1. data->Length() == 0: Retry later (EAGAIN)
		// 2-2. data->Length() > 0: Data is remained, so must call Recv() again to empty socket buffer
		//                          (If not, epoll event will not occur later)
		//
		// If MakeNonBlocking() is called, non_block is ignored
		std::shared_ptr<Error> Recv(std::shared_ptr<Data> &data, bool non_block = false);
		std::shared_ptr<Error> Recv(void *data, size_t length, size_t *received_length, bool non_block = false);

		// If MakeNonBlocking() is called, non_block is ignored
		std::shared_ptr<Error> RecvFrom(std::shared_ptr<Data> &data, SocketAddress *address, bool non_block = false);

		// Dispatches as many command as possible
		DispatchResult DispatchEvents();

		bool Flush();

		bool CloseIfNeeded();
		bool Close();

		bool HasCommand() const
		{
			return _dispatch_queue.size() > 0;
		}

		bool HasExpiredCommand() const
		{
			std::lock_guard lock_guard(_dispatch_queue_lock);

			if (HasCommand())
			{
				return _dispatch_queue.front().IsExpired(OV_SOCKET_EXPIRE_TIMEOUT);
			}

			return false;
		}

		bool IsEndOfStream() const
		{
			return _end_of_stream;
		}

		void SetEndOfStream()
		{
			_end_of_stream = true;
		}

		bool IsClosing() const
		{
			return _has_close_command;
		}

		virtual String ToString() const;

	protected:
		struct DispatchCommand
		{
			static constexpr int CLOSE_TYPE_MASK = 0x40;

			enum class Type : uint8_t
			{
				// Need to call connection callback
				Connected = 0x00,

				// Need to send data using send()
				Send = 0x01,
				// Need to send data using sendto()
				SendTo = 0x02,

				// Need to call shutdown(SHUT_WR) (TCP only)
				HalfClose = CLOSE_TYPE_MASK | 0x01,
				// Wait for ACK/FIN
				WaitForHalfClose = CLOSE_TYPE_MASK | 0x02,
				// Need to close the socket
				Close = CLOSE_TYPE_MASK | 0x03
			};

			static const char *StringFromType(Type type)
			{
				switch (type)
				{
					case Type::Connected:
						return "Connected";

					case Type::Send:
						return "Send";

					case Type::SendTo:
						return "SendTo";

					case Type::HalfClose:
						return "HalfClose";

					case Type::WaitForHalfClose:
						return "WaitForHalfClose";

					case Type::Close:
						return "Close";
				}

				return "Unknown";
			}

			DispatchCommand(const std::shared_ptr<const Data> &data)
				: type(Type::Send),
				  data(data),
				  enqueued_time(std::chrono::system_clock::now())
			{
			}

			DispatchCommand(const SocketAddress &address, const std::shared_ptr<const Data> &data)
				: type(Type::SendTo),
				  address(address),
				  data(data),
				  enqueued_time(std::chrono::system_clock::now())
			{
			}

			DispatchCommand(Type type)
				: type(type),
				  enqueued_time(std::chrono::system_clock::now())
			{
			}

			bool IsCloseCommand() const
			{
				return OV_CHECK_FLAG(static_cast<uint8_t>(type), CLOSE_TYPE_MASK);
			}

			void UpdateTime()
			{
				enqueued_time = std::chrono::system_clock::now();
			}

			bool IsExpired(int millisecond_time) const
			{
				auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - enqueued_time);

				return (delta.count() >= millisecond_time);
			}

			String ToString() const
			{
				return String::FormatString(
					"<DispatchCommand: %p, %s, type: %s%s%s, data: %zu bytes>",
					this,
					ov::Converter::ToISO8601String(enqueued_time).CStr(),
					StringFromType(type),
					(type == DispatchCommand::Type::SendTo) ? ", address: " : "",
					(type == DispatchCommand::Type::SendTo) ? address.ToString().CStr() : "",
					(data != nullptr) ? data->GetLength() : 0);
			}

			Type type = Type::Close;
			SocketAddress address;
			std::shared_ptr<const Data> data;
			std::chrono::time_point<std::chrono::system_clock> enqueued_time;
		};

	protected:
		virtual bool Create(SocketType type);

		bool SetBlockingInternal(bool blocking);

		bool AppendCommand(DispatchCommand command);

		DispatchResult DispatchInternal(DispatchCommand &command);

		ssize_t SendInternal(const std::shared_ptr<const Data> &data);
		ssize_t SendToInternal(const SocketAddress &address, const std::shared_ptr<const Data> &data);

		// From SocketPollWorker (Called when EPOLLIN event raised)
		virtual void OnReadableFromSocket()
		{
			if (_callback != nullptr)
			{
				_callback->OnReadable();
			}
		}

		std::shared_ptr<Error> RecvInternal(void *data, size_t length, size_t *received_length);

		virtual String ToString(const char *class_name) const;

		DispatchResult HalfClose();
		DispatchResult WaitForHalfClose();

		virtual bool CloseInternal();

	protected:
		std::shared_ptr<SocketPoolWorker> _worker;

		SocketWrapper _socket;

		SocketState _state = SocketState::Closed;

		bool _is_nonblock = true;

		bool _end_of_stream = false;

		std::shared_ptr<SocketAddress> _local_address = nullptr;
		std::shared_ptr<SocketAddress> _remote_address = nullptr;

		mutable std::recursive_mutex _dispatch_queue_lock;
		std::deque<DispatchCommand> _dispatch_queue;
		bool _has_close_command = false;

		std::atomic<bool> _connection_event_fired{false};
		std::shared_ptr<SocketAsyncInterface> _callback;

		volatile bool _force_stop = false;
	};
}  // namespace ov
