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

#include <functional>

namespace ov
{
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

	class Socket;

	// For TCP sockets
	class ServerSocket;
	class ClientSocket;

	typedef std::function<SocketConnectionState(const std::shared_ptr<ov::ClientSocket> &client, SocketConnectionState state, const std::shared_ptr<ov::Error> &error)> ClientConnectionCallback;
	typedef std::function<SocketConnectionState(const std::shared_ptr<ov::ClientSocket> &client, const std::shared_ptr<Data> &data)> ClientDataCallback;

	// for UDP socket
	class DatagramSocket;
	class SocketAddress;

	typedef std::function<void(const std::shared_ptr<ov::DatagramSocket> &client, const SocketAddress &remote_address, const std::shared_ptr<Data> &data)> DatagramCallback;

	const ssize_t TcpBufferSize = 4096;
	const ssize_t UdpBufferSize = 4096;
}  // namespace ov
