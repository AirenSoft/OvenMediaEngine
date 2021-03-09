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
#include "socket_datastructure.h"

namespace ov
{
	class DatagramSocket : public Socket, public SocketAsyncInterface
	{
	public:
		DatagramSocket(PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker)
			: Socket(token, worker)
		{
		}

		~DatagramSocket() override = default;

		// 특정 port로 bind
		bool Prepare(int port, DatagramCallback datagram_callback);
		// address에 해당하는 주소로 bind
		bool Prepare(const SocketAddress &address, DatagramCallback datagram_callback);

		using Socket::Close;
		using Socket::Connect;
		using Socket::GetState;
		using Socket::Recv;
		using Socket::RecvFrom;
		using Socket::Send;
		using Socket::SendTo;

		String ToString() const override;

	protected:
		//--------------------------------------------------------------------
		// Overriding of Socket
		//--------------------------------------------------------------------
		bool CloseInternal() override;
		
		//--------------------------------------------------------------------
		// Implementation of SocketAsyncInterface
		//--------------------------------------------------------------------
		void OnConnected() override
		{
			// datagram socket should not be called this event
			OV_ASSERT2(false);
		}
		void OnReadable() override;
		void OnClosed() override
		{
			// datagram socket should not be called this event
			OV_ASSERT2(false);
		}

		DatagramCallback _datagram_callback = nullptr;
	};
}  // namespace ov
