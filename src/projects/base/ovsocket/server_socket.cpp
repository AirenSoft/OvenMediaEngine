//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "server_socket.h"

#include <netinet/tcp.h>

#include "client_socket.h"
#include "socket_pool/socket_pool.h"
#include "socket_pool/socket_pool_worker.h"
#include "socket_private.h"

#undef OV_LOG_TAG
#define OV_LOG_TAG "Socket.Server"

#define logap(format, ...) logtp("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logad(format, ...) logtd("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logas(format, ...) logts("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)

#define logai(format, ...) logti("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logaw(format, ...) logtw("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logae(format, ...) logte("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)
#define logac(format, ...) logtc("[#%d] [%p] " format, (GetNativeHandle() == -1) ? 0 : GetNativeHandle(), this, ##__VA_ARGS__)

namespace ov
{
	ServerSocket::~ServerSocket()
	{
	}

	bool ServerSocket::Prepare(uint16_t port,
							   ClientConnectionCallback connection_callback,
							   ClientDataCallback data_callback,
							   int send_buffer_size,
							   int recv_buffer_size,
							   int backlog)
	{
		return Prepare(SocketAddress(port), connection_callback, data_callback, send_buffer_size, recv_buffer_size, backlog);
	}

	bool ServerSocket::Prepare(const SocketAddress &address,
							   ClientConnectionCallback connection_callback,
							   ClientDataCallback data_callback,
							   int send_buffer_size,
							   int recv_buffer_size,
							   int backlog)
	{
		CHECK_STATE(== SocketState::Created, false);

		if (
			(
				MakeNonBlocking(GetSharedPtrAs<SocketAsyncInterface>()) &&
				SetSocketOptions(send_buffer_size, recv_buffer_size) &&
				Bind(address) &&
				Listen(backlog)))
		{
			_connection_callback = connection_callback;
			_data_callback = data_callback;

			return true;
		}

		Close();

		return false;
	}

	void ServerSocket::AcceptClients()
	{
		while (true)
		{
			SocketAddress address;
			SocketWrapper client_socket = Socket::Accept(&address);

			if (client_socket.IsValid() == false)
			{
				// There is no client to accept
				return;
			}

			logad("Trying to allocate a socket for client: %s", address.ToString().CStr());

			auto client = _pool->AllocSocket<ClientSocket>(GetSharedPtrAs<ServerSocket>(), client_socket, address);

			if (client != nullptr)
			{
				logad("New client is connected: %s", client->ToString().CStr());

				if (client->Prepare())
				{
					// An event occurs the moment client socket is added to the epoll,
					// so we must add the client into _client_list before calling AttachToWorker()
					{
						std::lock_guard lock_guard(_client_list_mutex);
						_client_list[client.get()] = client;
					}

					if (client->AttachToWorker() == false)
					{
						// The client will be removed from _client_list in the future
						logad("An error occurred while attach client to worker: %s", client->ToString().CStr());
						client->Close();
					}
				}
				else
				{
					logae("Could not prepare socket options: %s", client->ToString().CStr());
				}
			}
		}
	}

	bool ServerSocket::OnClientDisconnected(const std::shared_ptr<ClientSocket> &client)
	{
		std::lock_guard lock_guard(_client_list_mutex);

		auto item = _client_list.find(client.get());

		if (item == _client_list.end())
		{
			return false;
		}

		logai("Client(%s) is disconnected", client->ToString().CStr());

		_client_list.erase(item);

		return true;
	}

	void ServerSocket::OnReadable()
	{
		AcceptClients();
	}

	bool ServerSocket::CloseInternal()
	{
		_client_list_mutex.lock();
		auto client_list = std::move(_client_list);
		_client_list_mutex.unlock();

		for (const auto &client : client_list)
		{
			client.second->Close();
		}

		_callback = nullptr;

		return Socket::CloseInternal();
	}

	String ServerSocket::ToString() const
	{
		return Socket::ToString("ServerSocket");
	}

	bool ServerSocket::SetSocketOptions(int send_buffer_size, int recv_buffer_size)
	{
		// SRT socket is already non-block mode
		bool result = true;

		switch (GetType())
		{
			case SocketType::Tcp: {
				result &= SetSockOpt<int>(SO_REUSEADDR, 1);

				// Disable Nagle's algorithm
				// result &= SetSockOpt<int>(IPPROTO_TCP, TCP_NODELAY, 1);

				// Set send buffer size if smaller than argument (0 == default)
				int current_send_buffer_size = 0;
				GetSockOpt(SO_SNDBUF, &current_send_buffer_size);
				if ((send_buffer_size != 0) && (current_send_buffer_size < send_buffer_size))
				{
					// We need to divide by 2, because the return value of SO_SNDBUF always (input value * 2)
					result &= SetSockOpt<int>(SO_SNDBUF, send_buffer_size / 2);

					GetSockOpt(SO_SNDBUF, &current_send_buffer_size);

					logad("TCP Server send buffer size (requested: %d, from socket: %d)", send_buffer_size * 2, current_send_buffer_size);
				}

				// Set recv buffer size if smaller than argument (0 == default)
				int current_recv_buffer_size = 0;
				GetSockOpt(SO_RCVBUF, &current_recv_buffer_size);
				if ((recv_buffer_size != 0) && (current_recv_buffer_size < recv_buffer_size))
				{
					// We need to divide by 2, because the return value of SO_RCVBUF always (input value * 2)
					result &= SetSockOpt<int>(SO_RCVBUF, recv_buffer_size / 2);

					GetSockOpt(SO_RCVBUF, &current_recv_buffer_size);

					logad("TCP Server recv buffer size (requested: %d, from socket: %d)", recv_buffer_size * 2, current_recv_buffer_size);
				}
				break;
			}

			case SocketType::Udp:
				// Nothing to do
				break;

			case SocketType::Srt:
				// https://github.com/Haivision/srt/blob/master/docs/API.md

				// OptName	        Since	Binding	    Type	    Units	    Default	    Range
				// SRTO_TRANSTYPE	1.3.0	pre	        enum		            SRTT_LIVE	alt: SRTT_FILE
				//
				// [SET] - Sets the transmission type for the socket, in particular,
				// setting this option sets multiple other parameters to their default values as required for a particular transmission type.
				//     SRTT_LIVE: Set options as for live transmission.
				//                In this mode, you should send by one sending instruction only so many data
				//                that fit in one UDP packet, and limited to the value defined first in SRTO_PAYLOADSIZE option
				//                (1316 is default in this mode). There is no speed control in this mode, only the bandwidth control, if configured, in order to not exceed the bandwidth with the overhead transmission (retransmitted and control packets).
				//     SRTT_FILE: Set options as for non-live transmission.
				//                See SRTO_MESSAGEAPI for further explanations
				result &= SetSockOpt<SRT_TRANSTYPE>(SRTO_TRANSTYPE, SRTT_LIVE);

				// OptName	        Since	Binding	    Type	    Units	    Default	    Range
				// SRTO_INPUTBW	    1.0.5	post	    int64_t	    bytes/s	    0	        0..
				//
				// Sender nominal input rate. Used along with OHEADBW, when MAXBW is set to relative (0),
				// to calculate maximum sending rate when recovery packets are sent along with main media stream (INPUTBW * (100 + OHEADBW) / 100).
				// If INPUTBW is not set while MAXBW is set to relative (0), the actual input rate is evaluated inside the library.
				result &= SetSockOpt<int64_t>(SRTO_INPUTBW, 10LL * (1024LL * (1024LL * 1024LL)));

				// OptName	        Since	Binding	    Type	    Units	    Default	    Range
				// SRTO_MAXBW	    1.0.5	pre	        int64_t	    bytes/sec	-1	    -1
				//
				// [GET or SET] - Maximum send bandwidth.
				// -1: infinite (CSRTCC limit is 30mbps) =
				// 0: relative to input rate (SRT 1.0.5 addition, see SRTO_INPUTBW)
				// SRT recommended value: 0 (relative)
				result &= SetSockOpt<int64_t>(SRTO_MAXBW, 0LL);

				// OptName	        Since	Binding	    Type	    Units	    Default	    Range
				// SRTO_OHEADBW     1.0.5	post	    int         %	        25	        5..100
				//
				// Recovery bandwidth overhead above input rate (see SRTO_INPUTBW).
				// Sender: user configurable, default: 25%.
				// To do: set-only. get should be supported.
				// SetSockOpt<int>(SRTO_OHEADBW, 100);

				// OptName	        Since	Binding	    Type	    Units	    Default     Range
				// SRTO_PEERLATENCY	1.3.0	pre	        int32_t	    msec	    0	        positive only
				//
				// The latency value (as described in SRTO_RCVLATENCY) that is set by the sender side as a minimum value for the receiver.
				result &= SetSockOpt<int32_t>(SRTO_PEERLATENCY, 50);

				// OptName          Since	Binding	    Type	    Units	    Default	    Range
				// SRTO_RCVLATENCY	1.3.0	pre	        int32_t	    msec	    0	        positive only
				//
				// The time that should elapse since the moment when the packet was sent and the moment
				// when it's delivered to the receiver application in the receiving function.
				// This time should be a buffer time large enough to cover the time spent for sending,
				// unexpectedly extended RTT time, and the time needed to retransmit the lost UDP packet.
				// The effective latency value will be the maximum of this options' value and the value of SRTO_PEERLATENCY set by the peer side.
				// This option in pre-1.3.0 version is available only as SRTO_LATENCY.
				result &= SetSockOpt<int32_t>(SRTO_RCVLATENCY, 50);

				// OptName	        Since	Binding	    Type	    Units	    Default	    Range
				// SRTO_RCVSYN		        pre	        bool    	true	    true	    false
				//
				// [GET or SET] - Synchronous (blocking) receive mode
				result &= SetSockOpt<bool>(SRTO_RCVSYN, false);

				// OptName	        Since	Binding	    Type	    Units	    Default	    Range
				// SRTO_SNDSYN		        post	    bool	    true	    true	    false
				//
				// [GET or SET] - Synchronous (blocking) send mode
				result &= SetSockOpt<bool>(SRTO_SNDSYN, false);

				// OptName          Since	Binding	    Type	    Units	    Default	    Range
				// SRTO_REUSEADDR	        pre			true	                true        false
				//
				// When true, allows the SRT socket use the binding address used already by another SRT socket in the same application.
				// Note that SRT socket use an intermediate object to access the underlying UDP sockets called Multiplexer,
				// so multiple SRT socket may share one UDP socket and the packets received to this UDP socket
				// will be correctly dispatched to the SRT socket to which they are currently destined.
				// This has some similarities to SO_REUSEADDR system socket option, although it's only used inside SRT.
				//
				// TODO: This option weirdly only allows the socket used in bind() to use the local address
				// that another socket is already using, but not to disallow another socket in the same application
				// to use the binding address that the current socket is already using.
				// What it actually changes is that when given address in bind() is already used by another socket,
				// this option will make the binding fail instead of making the socket added to the shared group of that
				// socket that already has bound this address - but it will not disallow another socket reuse its address.
				result &= SetSockOpt<bool>(SRTO_REUSEADDR, true);
				break;

			default:
				result = false;
		}

		return result;
	}
}  // namespace ov
