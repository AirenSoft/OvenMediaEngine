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
#include <shared_mutex>

namespace ov
{
	// TCP 서버 (UDP는 DatagramSocket 사용)
	class ServerSocket : public Socket
	{
	public:
		ServerSocket() = default;
		~ServerSocket() override;

		// 특정 port로 bind. backlog 지정 시, 해당 크기만큼 backlog 지정
		bool Prepare(SocketType type,
					 uint16_t port,
					 int send_buffer_size,
					 int recv_buffer_size,
					 int backlog = SOMAXCONN);

		// address에 해당하는 주소로 bind
		bool Prepare(SocketType type,
					 const SocketAddress &address,
					 int send_buffer_size,
					 int recv_buffer_size,
					 int backlog = SOMAXCONN);

		virtual bool DispatchEvent(ClientConnectionCallback connection_callback, ClientDataCallback data_callback, int timeout = Infinite);

		virtual std::shared_ptr<ClientSocket> Accept();

		bool Close() override;

		using Socket::GetState;
		using Socket::ToString;

		String ToString() const override;

		virtual bool DisconnectClient(std::shared_ptr<ClientSocket> client_socket, SocketConnectionState state, const std::shared_ptr<Error> &error = nullptr);
		virtual bool DisconnectClient(ClientSocket *client_socket, SocketConnectionState state, const std::shared_ptr<Error> &error = nullptr);

	protected:
		virtual bool SetSocketOptions(SocketType type, int send_buffer_size, int recv_buffer_size);

		void DispatchAccept();
		void DispatchEvents(const void *key, const epoll_event *event);

		std::shared_mutex _client_list_mutex;
		std::map<const void *, std::shared_ptr<ClientSocket>> _client_list;
		// To keep ClientSocket pointer while DispatchEvent() is running
		// (In DispatchEvent(), the client_socket is not referenced as shared_ptr)
		std::map<const void *, std::shared_ptr<ClientSocket>> _disconnected_client_list;

		ClientConnectionCallback _connection_callback = nullptr;
		ClientDataCallback _data_callback = nullptr;
	};
}  // namespace ov
