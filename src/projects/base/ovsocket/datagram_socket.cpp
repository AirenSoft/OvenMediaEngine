//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "datagram_socket.h"

#include "client_socket.h"
#include "socket_private.h"

#undef OV_LOG_TAG
#define OV_LOG_TAG "Socket.Datagram"

#define logad(format, ...) logtd("[%p] " format, this, ##__VA_ARGS__)
#define logas(format, ...) logts("[%p] " format, this, ##__VA_ARGS__)

#define logai(format, ...) logti("[%p] " format, this, ##__VA_ARGS__)
#define logaw(format, ...) logtw("[%p] " format, this, ##__VA_ARGS__)
#define logae(format, ...) logte("[%p] " format, this, ##__VA_ARGS__)
#define logac(format, ...) logtc("[%p] " format, this, ##__VA_ARGS__)

namespace ov
{
	bool DatagramSocket::Prepare(int port, DatagramCallback datagram_callback)
	{
		return Prepare(SocketAddress(port), std::move(datagram_callback));
	}

	bool DatagramSocket::Prepare(const SocketAddress &address, DatagramCallback datagram_callback)
	{
		CHECK_STATE(== SocketState::Created, false);

		if (
			(
				MakeNonBlocking(GetSharedPtrAs<ov::SocketAsyncInterface>()) &&
				SetSockOpt<int>(SO_REUSEADDR, 1) &&
				Bind(address)))
		{
			_datagram_callback = std::move(datagram_callback);

			return true;
		}

		Close();

		return false;
	}

	bool DatagramSocket::CloseInternal()
	{
		_callback = nullptr;

		return Socket::CloseInternal();
	}

	void DatagramSocket::OnReadable()
	{
		logtp("Trying to read UDP packets...");

		auto data = std::make_shared<ov::Data>(UdpBufferSize);

		while (true)
		{
			SocketAddress remote;
			auto error = RecvFrom(data, &remote);

			if (error == nullptr)
			{
				if (data->GetLength() == 0L)
				{
					// Try later
					break;
				}
				else
				{
					if (_datagram_callback != nullptr)
					{
						_datagram_callback(GetSharedPtrAs<DatagramSocket>(), remote, data->Clone());
					}
				}
			}
			else
			{
				// An error occurred
				break;
			}
		}
	}

	String DatagramSocket::ToString() const
	{
		return Socket::ToString("DatagramSocket");
	}
}  // namespace ov