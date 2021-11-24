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

#if DEBUG
// #	define CLIENT_SOCKET_SIMULATE_PACKET_FRAGMENT
#	define CLIENT_SOCKET_FRAGMENT_AMOUNT(remained) remained
// #	define CLIENT_SOCKET_FRAGMENT_AMOUNT(remained) 1
// #	define CLIENT_SOCKET_FRAGMENT_AMOUNT(remained) ov::Random::GenerateInt32(1, remained)
#endif

namespace ov
{
	ClientSocket::ClientSocket(
		PrivateToken token, const std::shared_ptr<SocketPoolWorker> &worker,
		const std::shared_ptr<ServerSocket> &server_socket, SocketWrapper client_socket, const SocketAddress &remote_address)
		: Socket(token, worker, client_socket, remote_address),

		  _server_socket(server_socket)
	{
		OV_ASSERT2(server_socket != nullptr);

		_local_address = (server_socket != nullptr) ? server_socket->GetLocalAddress() : nullptr;
	}

	ClientSocket::~ClientSocket()
	{
	}

	bool ClientSocket::Create(SocketType type)
	{
		auto server_socket = _server_socket.lock();

		if (server_socket != nullptr)
		{
			// Do not need to create a socket - socket is already created
			return true;
		}

		OV_ASSERT2(false);

		return false;
	}

	bool ClientSocket::GetSrtStreamId()
	{
		if (GetType() == ov::SocketType::Srt)
		{
			char stream_id_buff[512];
			int stream_id_len = sizeof(stream_id_buff);
			if (srt_getsockflag(GetNativeHandle(), SRT_SOCKOPT::SRTO_STREAMID, &stream_id_buff[0], &stream_id_len) != SRT_ERROR)
			{
				_stream_id = ov::String(stream_id_buff, stream_id_len);
				return true;
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	bool ClientSocket::SetSocketOptions()
	{
		bool result = true;

		switch (GetType())
		{
			case SocketType::Tcp:
				// Disable Nagle's algorithm
				result &= SetSockOpt<int>(IPPROTO_TCP, TCP_NODELAY, 1);
				// Quick ACK
				result &= SetSockOpt<int>(IPPROTO_TCP, TCP_QUICKACK, 1);
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
		// In the case of SRT, app/stream is classified by streamid.
		// Since the streamid is processed by the application, error is not checked here.
		GetSrtStreamId();

		return
			// Set socket options
			SetSocketOptions() &&
			AppendCommand({DispatchCommand::Type::Connected}) &&
			SetFirstEpollEventReceived() &&
			MakeNonBlockingInternal(GetSharedPtrAs<SocketAsyncInterface>(), false);
	}

	void ClientSocket::OnConnected(const std::shared_ptr<const SocketError> &error)
	{
		// Because ClientSocket is created by accepting from ServerSocket rather than actually connecting,
		// there should be no connection error.
		OV_ASSERT2(error == nullptr);

		auto server_socket = _server_socket.lock();

		if (server_socket == nullptr)
		{
			OV_ASSERT2("_server_socket must not be nullptr");
			return;
		}

		if (server_socket != nullptr)
		{
			auto callback = server_socket->GetConnectionCallback();

			if (callback != nullptr)
			{
				callback(GetSharedPtrAs<ClientSocket>(), SocketConnectionState::Connected, nullptr);
			}
		}
	}

	void ClientSocket::OnReadable()
	{
		auto server_socket = _server_socket.lock();

		if (server_socket == nullptr)
		{
			OV_ASSERT2("_server_socket must not be nullptr");
			return;
		}

		auto &data_callback = server_socket->GetDataCallback();

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
#ifdef CLIENT_SOCKET_SIMULATE_PACKET_FRAGMENT
						off_t offset = 0;
						size_t remained = data->GetLength();
						while (remained > 0)
						{
							auto length = CLIENT_SOCKET_FRAGMENT_AMOUNT(remained);

							data_callback(GetSharedPtrAs<ClientSocket>(), data->Subdata(offset, length)->Clone());

							offset += length;
							remained -= length;
						}
#else	// CLIENT_SOCKET_SIMULATE_PACKET_FRAGMENT
						data_callback(GetSharedPtrAs<ClientSocket>(), data->Clone());
#endif	// CLIENT_SOCKET_SIMULATE_PACKET_FRAGMENT
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
		auto server_socket = _server_socket.lock();

		if (server_socket == nullptr)
		{
			OV_ASSERT2("_server_socket must not be nullptr");
			return;
		}

		auto callback = server_socket->GetConnectionCallback();

		if (callback != nullptr)
		{
			auto state = (GetState() == SocketState::Disconnected) ? (SocketConnectionState::Disconnected) : (SocketConnectionState::Disconnect);

			callback(GetSharedPtrAs<ClientSocket>(), state, nullptr);
		}

		server_socket->OnClientDisconnected(GetSharedPtrAs<ClientSocket>());
	}

	bool ClientSocket::CloseInternal()
	{
		auto server_socket = _server_socket.lock();

		if (server_socket == nullptr)
		{
			OV_ASSERT2("_server_socket must not be nullptr");
			return false;
		}

		if (Socket::CloseInternal())
		{
			SetState(SocketState::Closed);
		}

		return server_socket->OnClientDisconnected(GetSharedPtrAs<ClientSocket>());
	}

	String ClientSocket::ToString() const
	{
		return Socket::ToString("ClientSocket");
	}
}  // namespace ov
