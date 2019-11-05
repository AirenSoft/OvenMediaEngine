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

namespace ov
{
	ClientSocket::ClientSocket(ServerSocket *server_socket)
		: Socket(),

		  _server_socket(server_socket)
	{
		OV_ASSERT2(_server_socket != nullptr);
	}

	ClientSocket::ClientSocket(ServerSocket *server_socket, SocketWrapper socket, const SocketAddress &remote_address)
		: Socket(socket, remote_address),

		  _server_socket(server_socket)
	{
		OV_ASSERT2(_server_socket != nullptr);

		MakeNonBlocking();
	}

	ssize_t ClientSocket::Send(const ov::String &string, bool include_null_char)
	{
		return Socket::Send(string.CStr(), static_cast<size_t>(string.GetLength() + (include_null_char ? 1 : 0)));
	}

	bool ClientSocket::Close()
	{
		if (GetState() != SocketState::Closed)
		{
			return _server_socket->DisconnectClient(this->GetSharedPtrAs<ClientSocket>(), SocketConnectionState::Disconnected);
		}

		return true;
	}

	String ClientSocket::ToString() const
	{
		return Socket::ToString("ClientSocket");
	}
}  // namespace ov
