//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "client_socket.h"

#include "server_socket.h"
#include "socket_private.h"

#undef OV_LOG_TAG
#define OV_LOG_TAG "Socket.Client"

// If no packet is sent during this time, the connection is disconnected
#define CLIENT_SOCKET_SEND_TIMEOUT (60 * 1000)

namespace ov
{
	ClientSocket::ClientSocket(
		PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker,
		const std::shared_ptr<ServerSocket> &server_socket, SocketWrapper client_socket, const SocketAddress &remote_address)
		: Socket(token, worker, client_socket, remote_address),

		  _server_socket(server_socket)
	{
		OV_ASSERT2(_server_socket != nullptr);

		_local_address = (server_socket != nullptr) ? server_socket->GetLocalAddress() : nullptr;
	}

	ClientSocket::~ClientSocket()
	{
	}

	bool ClientSocket::Create(SocketType type)
	{
		if (_server_socket != nullptr)
		{
			// Do not need to create a socket - socket is already created
			return true;
		}

		OV_ASSERT2(false);

		return false;
	}

	bool ClientSocket::SetSocketOptions()
	{
		bool result = true;

		switch (GetType())
		{
			case SocketType::Tcp:
				// Enable TCP keep-alive
				result &= SetSockOpt<int>(SOL_SOCKET, SO_KEEPALIVE, 1);
				// Wait XX seconds before starting to determine that the connection is alive
				result &= SetSockOpt<int>(SOL_TCP, TCP_KEEPIDLE, 30);
				// Period of sending probe packet to determine keep alive
				result &= SetSockOpt<int>(SOL_TCP, TCP_KEEPINTVL, 10);
				// Number of times to probe
				result &= SetSockOpt<int>(SOL_TCP, TCP_KEEPCNT, 3);
				break;

			case SocketType::Udp:
				// Nothing to do
				break;

			case SocketType::Srt:
				// Nothing to do
				break;

			default:
				result = false;
				OV_ASSERT2(false);
				break;
		}

		return result;
	}

	bool ClientSocket::Prepare()
	{
		return
			// Set socket options
			SetSocketOptions() &&
			MakeNonBlocking(_server_socket);
	}

	bool ClientSocket::CloseInternal()
	{
		Socket::CloseInternal();
		
		return _server_socket->DisconnectClient(this->GetSharedPtrAs<ClientSocket>(), SocketConnectionState::Disconnect);
	}

	String ClientSocket::ToString() const
	{
		return Socket::ToString("ClientSocket");
	}
}  // namespace ov
