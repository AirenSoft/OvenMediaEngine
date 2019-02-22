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
#include "socket_address.h"
#include "socket_datastructure.h"

namespace ov
{
	// TCP 서버 (UDP는 DatagramSocket 사용)
	class ServerSocket : public Socket
	{
	public:
		ServerSocket() = default;
		~ServerSocket() override = default;

		// 특정 port로 bind. backlog 지정 시, 해당 크기만큼 backlog 지정
		bool Prepare(SocketType type, uint16_t port, int backlog = SOMAXCONN);

		// address에 해당하는 주소로 bind
		bool Prepare(SocketType type, const SocketAddress &address, int backlog = SOMAXCONN);

		bool DispatchEvent(ClientConnectionCallback connection_callback, ClientDataCallback data_callback, int timeout = Infinite);

		std::shared_ptr<ClientSocket> Accept();
		bool Close() override;

		using Socket::GetState;
		using Socket::ToString;

		String ToString() const override;

		bool RemoveClientSocket(ClientSocket * client_socket);

	protected:
		bool SetSocketOptions(SocketType type);

		std::map<ClientSocket *, std::shared_ptr<ClientSocket>> _client_list;
        std::mutex _client_list_mutex;
	};
}
