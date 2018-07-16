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
	// TCP socket
	class ServerSocket;
	class ClientSocket;

	enum class SocketConnectionState : int8_t
	{
		Connected,
		// 연결이 끊어짐
			Disconnected,
		// 오류가 발생해서 연결이 끊어짐
			Error
	};

	enum class SocketFamily : sa_family_t
	{
		Unknown = AF_UNSPEC,

		Inet = AF_INET,
		Inet6 = AF_INET6,
	};

	typedef std::function<bool(ClientSocket *client, SocketConnectionState state)> ClientConnectionCallback;
	typedef std::function<bool(ClientSocket *client, const std::shared_ptr<Data> &data)> ClientDataCallback;

	// UDP socket
	class DatagramSocket;
	class SocketAddress;

	typedef std::function<void(DatagramSocket *socket, const SocketAddress &remote_address, const std::shared_ptr<Data> &data)> DatagramCallback;

	const ssize_t TcpBufferSize = 4096;
	const ssize_t UdpBufferSize = 4096;
}
