//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "client_socket.h"

namespace ov
{
	ClientSocket::ClientSocket()
	{
	}

	ClientSocket::ClientSocket(SocketType type, socket_t socket, const sockaddr_in &remote_addr_in)
		: Socket(type, socket, remote_addr_in)
	{
		MakeNonBlocking();
	}

	ClientSocket::~ClientSocket()
	{
	}

	ssize_t ClientSocket::Send(const ov::String &string, bool include_null_char)
	{
		return Send((const void *)string.CStr(), string.GetLength() + (include_null_char ? 1 : 0));
	}

	String ClientSocket::ToString() const
	{
		return Socket::ToString("ClientSocket");
	}
}
