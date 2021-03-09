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
	class SocketPool;

	// TCP 서버 (UDP는 DatagramSocket 사용)
	class ServerSocket : public Socket, public SocketAsyncInterface
	{
	protected:
		friend class ClientSocket;

	public:
		ServerSocket(PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker, const std::shared_ptr<SocketPool> &pool)
			: Socket(token, worker),

			  _pool(pool)
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

	protected:
		bool SetSocketOptions(int send_buffer_size, int recv_buffer_size);

		ClientConnectionCallback &GetConnectionCallback()
		{
			return _connection_callback;
		}

		ClientDataCallback &GetDataCallback()
		{
			return _data_callback;
		}

		void AcceptClients();

		// Called from ClientSocket
		bool OnClientDisconnected(const std::shared_ptr<ClientSocket> &client);

		//--------------------------------------------------------------------
		// Overriding of Socket
		//--------------------------------------------------------------------
		bool CloseInternal() override;

		//--------------------------------------------------------------------
		// Implementation of SocketAsyncInterface
		//--------------------------------------------------------------------
		void OnConnected() override
		{
			// server socket should not be called this event
			OV_ASSERT2(false);
		}
		void OnReadable() override;
		void OnClosed() override
		{
			// server socket should not be called this event
			OV_ASSERT2(false);
		}

		std::shared_ptr<SocketPool> _pool;

		std::mutex _client_list_mutex;
		std::map<const void *, std::shared_ptr<ClientSocket>> _client_list;

		ClientConnectionCallback _connection_callback = nullptr;
		ClientDataCallback _data_callback = nullptr;
	};
}  // namespace ov
