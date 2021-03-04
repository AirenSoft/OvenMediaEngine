//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <shared_mutex>

#include "socket.h"
#include "socket_address.h"
#include "socket_datastructure.h"

namespace ov
{
	// TCP 서버 (UDP는 DatagramSocket 사용)
	class ServerSocket : public Socket, public SocketAsyncInterface
	{
	public:
		ServerSocket(PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker)
			: Socket(token, worker)
		{
		}

		~ServerSocket() override;

		// Bind to a specific port
		// When specifying a backlog, specify the size of the backlog
		bool Prepare(uint16_t port,
					 ClientConnectionCallback connection_callback,
					 ClientDataCallback data_callback,
					 int send_buffer_size,
					 int recv_buffer_size,
					 int backlog = SOMAXCONN);

		// Bind to the IP and port that the address points to
		// When specifying a backlog, specify the size of the backlog
		bool Prepare(const SocketAddress &address,
					 ClientConnectionCallback connection_callback,
					 ClientDataCallback data_callback,
					 int send_buffer_size,
					 int recv_buffer_size,
					 int backlog = SOMAXCONN);

		std::shared_ptr<ClientSocket> Accept();

		String ToString() const override;

		bool DisconnectClient(std::shared_ptr<ClientSocket> client_socket, SocketConnectionState state, const std::shared_ptr<Error> &error = nullptr);
		bool DisconnectClient(ClientSocket *client_socket, SocketConnectionState state, const std::shared_ptr<Error> &error = nullptr);

	protected:
		bool SetSocketOptions(int send_buffer_size, int recv_buffer_size);
		
		//--------------------------------------------------------------------
		// Overriding of Socket
		//--------------------------------------------------------------------
		bool CloseInternal() override;

		//--------------------------------------------------------------------
		// Implementation of SocketAsyncInterface
		//--------------------------------------------------------------------
		void OnReadable(const std::shared_ptr<ov::Socket> &socket) override;


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
