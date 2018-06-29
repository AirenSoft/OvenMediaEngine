//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "socket.h"

namespace ov
{
	// 일반적으로 사용되는 소켓 (client socket)
	class ClientSocket : public Socket
	{
	public:
		friend class ServerSocket;

		ClientSocket();
		ClientSocket(SocketType type, socket_t socket, const sockaddr_in &remote_addr_in);
		~ClientSocket() override;

		// 데이터 송신
		ssize_t Send(const ov::String &string, bool include_null_char = false);

		template<typename T>
		bool Send(const T *data)
		{
			ssize_t sent = Send(data, sizeof(T));
			bool result = sent == sizeof(T);

			OV_ASSERT(sent == sizeof(T), "Could not send data: sent: %ld, expected: %ld", sent, sizeof(T));

			return result;
		}

		using Socket::Connect;
		using Socket::Send;
		using Socket::Recv;
		using Socket::Close;
		using Socket::GetState;

		String ToString() const override;

	protected:
	};
}
