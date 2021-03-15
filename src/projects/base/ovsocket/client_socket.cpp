//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "client_socket.h"

#include "server_socket.h"
#include "socket_private.h"

#undef OV_LOG_TAG
#define OV_LOG_TAG "Socket.Client"

#define logap(format, ...) logtp("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logad(format, ...) logtd("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logas(format, ...) logts("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)

#define logai(format, ...) logti("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logaw(format, ...) logtw("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logae(format, ...) logte("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logac(format, ...) logtc("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)

// If no packet is sent during this time, the connection is disconnected
#define CLIENT_SOCKET_SEND_TIMEOUT (60 * 1000)

namespace ov
{
	ClientSocket::ClientSocket(
		PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker,
		const std::shared_ptr<ServerSocket> &server_socket, SocketWrapper client_socket, const SocketAddress &remote_address)
		: Socket(token, worker, client_socket, remote_address),

		  _server_socket(server_socket)
	{
		OV_ASSERT2(_server_socket != nullptr);

		_local_address = (server_socket != nullptr) ? server_socket->GetLocalAddress() : nullptr;
	}

	ClientSocket::~ClientSocket()
	{
	}

	bool ClientSocket::Create(SocketType type)
	{
		if (_server_socket != nullptr)
		{
			// Do not need to create a socket - socket is already created
			return true;
		}

		OV_ASSERT2(false);

		return false;
	}

	bool ClientSocket::SetSocketOptions()
	{
		bool result = true;

		switch (GetType())
		{
			case SocketType::Tcp:
				// Enable TCP keep-alive
				result &= SetSockOpt<int>(SOL_SOCKET, SO_KEEPALIVE, 1);
				// Wait XX seconds before starting to determine that the connection is alive
				result &= SetSockOpt<int>(SOL_TCP, TCP_KEEPIDLE, 30);
				// Period of sending probe packet to determine keep alive
				result &= SetSockOpt<int>(SOL_TCP, TCP_KEEPINTVL, 10);
				// Number of times to probe
				result &= SetSockOpt<int>(SOL_TCP, TCP_KEEPCNT, 3);
				break;

			case SocketType::Udp:
				// Nothing to do
				break;

			case SocketType::Srt:
				// Nothing to do
				break;

			default:
				result = false;
				OV_ASSERT2(false);
				break;
		}

		return result;
	}

	bool ClientSocket::Prepare()
	{
		return
			// Set socket options
			SetSocketOptions() &&
			MakeNonBlocking(GetSharedPtrAs<SocketAsyncInterface>()) &&
			AppendCommand({DispatchCommand::Type::Connected});
	}

	void ClientSocket::OnConnected()
	{
		auto callback = _server_socket->GetConnectionCallback();

		if (callback != nullptr)
		{
			callback(GetSharedPtrAs<ClientSocket>(), SocketConnectionState::Connected, nullptr);
		}
	}

	void ClientSocket::OnReadable()
	{
		auto &data_callback = _server_socket->GetDataCallback();

		auto data = std::make_shared<Data>(TcpBufferSize);

		while (true)
		{
			auto error = Recv(data);

			if (error == nullptr)
			{
				if (data->GetLength() > 0)
				{
					if (data_callback != nullptr)
					{
						data_callback(GetSharedPtrAs<ClientSocket>(), data->Clone());
					}

					continue;
				}
				else
				{
					// Try later (EAGAIN)
				}
			}
			else
			{
				// An error occurred
			}

			break;
		}
	}

	void ClientSocket::OnClosed()
	{
		auto callback = _server_socket->GetConnectionCallback();

		if (callback != nullptr)
		{
			callback(GetSharedPtrAs<ClientSocket>(), SocketConnectionState::Disconnected, nullptr);
		}

		_server_socket->OnClientDisconnected(GetSharedPtrAs<ClientSocket>());
	}

	bool ClientSocket::CloseInternal()
	{
		Socket::CloseInternal();

		return _server_socket->OnClientDisconnected(GetSharedPtrAs<ClientSocket>());
	}

	String ClientSocket::ToString() const
	{
		return Socket::ToString("ClientSocket");
	}
}  // namespace ov
