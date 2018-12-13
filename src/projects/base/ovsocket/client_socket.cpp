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
		: Socket()
	{
	}

	ClientSocket::ClientSocket(SocketWrapper socket, const SocketAddress &remote_address)
		: Socket(socket, remote_address)
	{
		MakeNonBlocking();
	}

	ssize_t ClientSocket::Send(const ov::String &string, bool include_null_char)
	{
		return Socket::Send(string.CStr(), static_cast<size_t>(string.GetLength() + (include_null_char ? 1 : 0)));
	}

	String ClientSocket::ToString() const
	{
		return Socket::ToString("ClientSocket");
	}
}
