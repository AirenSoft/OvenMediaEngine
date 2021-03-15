//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <arpa/inet.h>
#include <base/ovlibrary/ovlibrary.h>
#include <srt/srt.h>
#include <sys/epoll.h>

#include <functional>

#include "epoll_wrapper.h"

#define OV_SOCKET_ADD_FLAG_IF(list, x, flag) \
	if (OV_CHECK_FLAG(x, flag))              \
	{                                        \
		list.emplace_back(#flag);            \
	}

#define OV_SOCKET_DECLARE_PRIVATE_TOKEN()   \
	struct PrivateToken                     \
	{                                       \
		explicit PrivateToken(const void *) \
		{                                   \
		}                                   \
	}

namespace ov
{
	// socket type
	typedef int socket_t;

	// A socket value when not initialized
	const int InvalidSocket = -1;
	static_assert(SRT_INVALID_SOCK == InvalidSocket, "SRT_INVALID_SOCK must be equal to InvalidSocket");

	constexpr const int EpollMaxEvents = 1024;
	constexpr const int MaxSrtPacketSize = 1316;

	const ssize_t TcpBufferSize = 4096;
	const ssize_t UdpBufferSize = 4096;

	enum class SocketConnectionState : int8_t
	{
		/// Socket is connected
		Connected,
		/// Disconnected by server
		Disconnect,
		/// Disconnected from client
		Disconnected,
		/// An error occurred
		Error
	};

	enum class SocketFamily : sa_family_t
	{
		Unknown = AF_UNSPEC,

		Inet = AF_INET,
		Inet6 = AF_INET6,
	};

	enum class SocketType : char
	{
		Unknown,
		Udp,
		Tcp,
		Srt
	};

	enum class SocketState : char
	{
		// Socket was closed
		Closed,
		// Socket is created
		Created,
		// Bound on some port
		Bound,
		// Listening
		Listening,
		// Connection established
		Connected,
		// The connection with Peer has been lost (However, we can read data from the kernel socket buffer if available)
		Disconnected,
		// An error occurred
		Error,
	};

	static const char *StringFromSocketState(SocketState state)
	{
		switch (state)
		{
			case SocketState::Closed:
				return "Closed";

			case SocketState::Created:
				return "Created";

			case SocketState::Bound:
				return "Bound";

			case SocketState::Listening:
				return "Listening";

			case SocketState::Connected:
				return "Connected";

			case SocketState::Disconnected:
				return "Disconnected";

			case SocketState::Error:
				return "Error";

			default:
				return "Unknown";
		}
	}

	static const char *StringFromSocketType(SocketType type)
	{
		switch (type)
		{
			case SocketType::Udp:
				return "UDP";

			case SocketType::Tcp:
				return "TCP";

			case SocketType::Srt:
				return "SRT";

			case SocketType::Unknown:
			default:
				return "Unknown";
		}
	}

	class SocketAddress;

	// For TCP sockets
	class ServerSocket;
	class ClientSocket;

	typedef std::function<void(const std::shared_ptr<ov::ClientSocket> &client, SocketConnectionState state, const std::shared_ptr<ov::Error> &error)> ClientConnectionCallback;
	typedef std::function<void(const std::shared_ptr<ov::ClientSocket> &client, const std::shared_ptr<Data> &data)> ClientDataCallback;

	// For UDP sockets
	class DatagramSocket;

	typedef std::function<void(const std::shared_ptr<ov::DatagramSocket> &client, const SocketAddress &remote_address, const std::shared_ptr<Data> &data)> DatagramCallback;

	static String StringFromEpollEvent(const epoll_event &event)
	{
		std::vector<String> flags;

		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLIN);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLPRI);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLOUT);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLRDNORM);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLRDBAND);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLWRNORM);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLWRBAND);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLMSG);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLERR);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLHUP);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLRDHUP);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLWAKEUP);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLONESHOT);
		OV_SOCKET_ADD_FLAG_IF(flags, event.events, EPOLLET);

		return ov::String::Join(flags, " | ");
	}

	static String StringFromEpollEvent(const epoll_event *event)
	{
		return StringFromEpollEvent(*event);
	}
}  // namespace ov
