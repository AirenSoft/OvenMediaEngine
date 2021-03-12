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
	// 일반적으로 사용되는 소켓 (server에서 생성한 client socket)
	class ClientSocket : public Socket, public SocketAsyncInterface
	{
	public:
		friend class ServerSocket;

		ClientSocket(
			PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker,
			const std::shared_ptr<ServerSocket> &server_socket, SocketWrapper client_socket, const SocketAddress &remote_address);

		~ClientSocket() override;

		bool Prepare();

		String ToString() const override;

	protected:
		bool Create(SocketType type) override;

		bool SetSocketOptions();

		//--------------------------------------------------------------------
		// Implementation of SocketAsyncInterface
		//--------------------------------------------------------------------
		void OnConnected() override;
		void OnReadable() override;
		void OnClosed() override;

		bool CloseInternal() override;

		std::shared_ptr<ServerSocket> _server_socket;
	};
}  // namespace ov
