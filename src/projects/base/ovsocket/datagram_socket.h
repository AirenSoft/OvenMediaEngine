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
	class DatagramSocket : public Socket
	{
	public:
		DatagramSocket() = default;
		~DatagramSocket() override = default;

		// 특정 port로 bind
		bool Prepare(int port);
		// address에 해당하는 주소로 bind
		bool Prepare(const SocketAddress &address);

		bool DispatchEvent(const DatagramCallback& data_callback, int timeout = Infinite);

		using Socket::Connect;
		using Socket::GetState;
		using Socket::Recv;
		using Socket::RecvFrom;
		using Socket::Send;
		using Socket::SendTo;
		using Socket::Close;

		String ToString() const override;

	protected:
	};
}
