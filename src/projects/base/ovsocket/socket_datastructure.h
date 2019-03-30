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

#include <functional>

#include <base/ovlibrary/ovlibrary.h>

namespace ov
{
	enum class SocketConnectionState : int8_t
	{
		Connected,
		Disconnected,                   // 연결이 끊어짐
		Error                           // 오류가 발생해서 연결이 끊어짐
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

	typedef std::function<bool(const std::shared_ptr<ov::ClientSocket> &client, SocketConnectionState state)> ClientConnectionCallback;
	typedef std::function<bool(const std::shared_ptr<ov::ClientSocket> &client, const std::shared_ptr<Data> &data)> ClientDataCallback;

	// for UDP socket
	class DatagramSocket;
	class SocketAddress;

	typedef std::function<void(const std::shared_ptr<ov::DatagramSocket> &client, const SocketAddress &remote_address, const std::shared_ptr<Data> &data)> DatagramCallback;

	const ssize_t TcpBufferSize = 4096;
	const ssize_t UdpBufferSize = 4096;
}
