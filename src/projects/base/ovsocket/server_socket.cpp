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

		_connection_callback = connection_callback;
		_data_callback = data_callback;

		if (
			(
				MakeNonBlocking(GetSharedPtrAs<ov::SocketAsyncInterface>()) &&
				SetSocketOptions(send_buffer_size, recv_buffer_size) &&
				Bind(address) &&
				Listen(backlog)) == false)
		{
			Close();

			// Rollback
			_connection_callback = nullptr;
			_data_callback = nullptr;

			return false;
		}

		return true;
	}

	void ServerSocket::OnReadable(const std::shared_ptr<ov::Socket> &socket)
	{
		if (socket.get() == this)
		{
			DispatchAccept();
		}
		else
		{
			auto data = std::make_shared<Data>(TcpBufferSize);

			while (true)
			{
				auto error = socket->Recv(data);

				if (error == nullptr)
				{
					if (data->GetLength() == 0)
					{
						// Try later
						break;
					}
					else
					{
						if (_data_callback != nullptr)
						{
							_data_callback(socket->GetSharedPtrAs<ClientSocket>(), data->Clone());
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

		// Delete all lists of clients that were temporarily holding std::shared_ptr<ClientSocket>
		// so that they could not be released while receiving data.
		//
		// TODO(dimiden): Since SocketPool currently manages Socket's life-cycle, let's consider deleting this later.
		{
			std::lock_guard<std::shared_mutex> lock(_client_list_mutex);
			_disconnected_client_list.clear();
		}
	}

	std::shared_ptr<ClientSocket> ServerSocket::Accept()
	{
		SocketAddress address;
		SocketWrapper client_socket = Socket::Accept(&address);

		if (client_socket.IsValid() == false)
		{
			return nullptr;
		}

		auto client = _worker->AllocSocket<ClientSocket>(GetSharedPtrAs<ServerSocket>(), client_socket, address);

		if (client != nullptr)
		{
			logad("New client is connected: %s", client->ToString().CStr());

			if (client->Prepare())
			{
				_client_list_mutex.lock();
				_client_list[client.get()] = client;
				_client_list_mutex.unlock();

				return client;
			}
			else
			{
				logae("Could not prepare socket options: %s", client->ToString().CStr());
			}
		}

		return nullptr;
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

	void ServerSocket::DispatchAccept()
	{
		while (true)
		{
			std::shared_ptr<ClientSocket> client = Accept();

			if (client == nullptr)
			{
				break;
			}

			logad("Client #%d is connected", client->GetSocket().GetNativeHandle());

			auto new_state = _connection_callback(client, SocketConnectionState::Connected, nullptr);

			switch (new_state)
			{
				case SocketConnectionState::Connected:
					break;

				case SocketConnectionState::Disconnect:
					logad("The connection callback requested to disconnect the client #%d",
						  client->GetSocket().GetNativeHandle());
					DisconnectClient(client, new_state);
					break;

				case SocketConnectionState::Disconnected:
					logad("Invalid socket state for client #%d",
						  client->GetSocket().GetNativeHandle());
					OV_ASSERT2(false);
					DisconnectClient(client, new_state);
					break;

				case SocketConnectionState::Error: {
					auto error = Error::CreateError("Connection", "The connection callback requested to disconnect the client #%d",
													_socket.GetNativeHandle(), client->GetSocket().GetNativeHandle());
					logad("%s", error->ToString().CStr());
					DisconnectClient(client, new_state, error);
					break;
				}
			}
		}
	}

	void ServerSocket::DispatchEvents(const void *key, const epoll_event *event)
	{
		std::shared_ptr<ClientSocket> client = nullptr;
		uint32_t epoll_events = event->events;

		{
			std::shared_lock<std::shared_mutex> lock(_client_list_mutex);
			auto item = _client_list.find(key);

			if (item == _client_list.end())
			{
				// If the client deleted from another thread as soon as the event occurs at epoll(), it enters here
				logad("Could not find a client: %p", key);
				return;
			}

			client = item->second;
		}

		if (OV_CHECK_FLAG(epoll_events, EPOLLERR) || (!OV_CHECK_FLAG(epoll_events, EPOLLIN)))
		{
			// An error occurred while communiting with the client
			auto error = Error::CreateError("Epoll", "%s", StringFromEpollEvent(event).CStr());
			logae("%s", error->ToString().CStr());

			DisconnectClient(client, SocketConnectionState::Error, error);
		}
		else if (OV_CHECK_FLAG(event->events, EPOLLHUP) || OV_CHECK_FLAG(event->events, EPOLLRDHUP))
		{
			// The client is disconnected
			logad("Client #%d is disconnected with events: %s", client->GetSocket().GetNativeHandle(), StringFromEpollEvent(event).CStr());
			DisconnectClient(client, SocketConnectionState::Disconnected);
		}
		else
		{
			// Data is available that sent by the client
			auto data = std::make_shared<Data>(TcpBufferSize);

			logad("The data received from client #%d", client->GetSocket().GetNativeHandle());

			while (client->GetState() == SocketState::Connected)
			{
				data->SetLength(0);

				auto error = client->Recv(data);

				if (data->GetLength() > 0L)
				{
					auto new_state = _data_callback(client->GetSharedPtrAs<ClientSocket>(), data);
					switch (new_state)
					{
						case SocketConnectionState::Connected:
							break;

						case SocketConnectionState::Disconnect:
							logad("The data callback requested to disconnect the client #%d",
								  client->GetSocket().GetNativeHandle());
							DisconnectClient(client, new_state);
							break;

						case SocketConnectionState::Disconnected:
							logad("Invalid socket state for client #%d",
								  client->GetSocket().GetNativeHandle());
							OV_ASSERT2(false);
							DisconnectClient(client, new_state);
							break;

						case SocketConnectionState::Error: {
							auto error = Error::CreateError("Connection", "The connection callback requested to disconnect the client #%d",
															_socket.GetNativeHandle(), client->GetSocket().GetNativeHandle());
							logad("%s", error->ToString().CStr());
							DisconnectClient(client, new_state, error);
							break;
						}
					}
				}

				if (client->GetState() == SocketState::Error)
				{
					logad("An error occurred on client %s", client->ToString().CStr());
					DisconnectClient(client, SocketConnectionState::Error, error);
					break;
				}

				if (error != nullptr)
				{
					logad("Client %s is disconnected", client->ToString().CStr());
					DisconnectClient(client, SocketConnectionState::Disconnected, nullptr);
					break;
				}

				// TODO(dimiden): (ClientSocketBlocking) Temporarily comment while processing as blocking
				// if (data->GetLength() == 0L)
				{
					// Waiting for next data
					break;
				}
			}
		}
	}

	String ServerSocket::ToString() const
	{
		return Socket::ToString("ServerSocket");
	}

	bool ServerSocket::DisconnectClient(std::shared_ptr<ClientSocket> client_socket, SocketConnectionState state, const std::shared_ptr<Error> &error)
	{
		if (client_socket == nullptr)
		{
			OV_ASSERT2(false);
			return false;
		}

		bool remove = false;

		{
			std::lock_guard<std::shared_mutex> lock(_client_list_mutex);

			auto item = _client_list.find(client_socket.get());

			if (item != _client_list.end())
			{
				logad("Deleting the client %s from list...", client_socket->ToString().CStr());
				_disconnected_client_list[item->first] = item->second;
				_client_list.erase(item);

				remove = true;
			}
			else
			{
				logad("Could not find socket instance for %s", client_socket->ToString().CStr());
			}
		}

		if (remove)
		{
			if (_connection_callback != nullptr)
			{
				_connection_callback(client_socket->GetSharedPtrAs<ClientSocket>(), state, error);
			}

			if (client_socket->GetState() != SocketState::Closed)
			{
				return _worker->ReleaseSocket(client_socket);
			}
		}
		else
		{
			return true;
		}

		return false;
	}

	bool ServerSocket::DisconnectClient(ClientSocket *client_socket, SocketConnectionState state, const std::shared_ptr<Error> &error)
	{
		if (client_socket != nullptr)
		{
			return DisconnectClient(client_socket->GetSharedPtrAs<ClientSocket>(), state, error);
		}

		OV_ASSERT2(false);
		return false;
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
