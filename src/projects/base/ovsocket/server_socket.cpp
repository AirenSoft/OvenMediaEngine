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
#include "socket_private.h"

namespace ov
{
	ServerSocket::~ServerSocket()
	{
	}

	bool ServerSocket::Prepare(SocketType type,
							   uint16_t port,
							   int send_buffer_size,
							   int recv_buffer_size,
							   int backlog)
	{
		return Prepare(type, SocketAddress(port), send_buffer_size, recv_buffer_size, backlog);
	}

	bool ServerSocket::Prepare(SocketType type,
							   const SocketAddress &address,
							   int send_buffer_size,
							   int recv_buffer_size,
							   int backlog)
	{
		CHECK_STATE(== SocketState::Closed, false);

		if (
			(
				Create(type) &&
				MakeNonBlocking() &&
				PrepareEpoll() &&
				AddToEpoll(this, static_cast<void *>(this)) &&
				SetSocketOptions(type, send_buffer_size, recv_buffer_size) &&
				Bind(address) &&
				Listen(backlog)) == false)
		{
			// 중간 과정에서 오류가 발생하면 실패 반환
			RemoveFromEpoll(this);
			Close();
			return false;
		}

		return true;
	}

	bool ServerSocket::DispatchEvent(ClientConnectionCallback connection_callback, ClientDataCallback data_callback, int timeout)
	{
		CHECK_STATE(== SocketState::Listening, false);
		OV_ASSERT2(connection_callback != nullptr);
		OV_ASSERT2(data_callback != nullptr);

		_connection_callback = connection_callback;
		_data_callback = data_callback;

		int count = EpollWait(timeout);

		for (int index = 0; index < count; index++)
		{
			const epoll_event *event = EpollEvents(index);

			if (event == nullptr)
			{
				// event must be not nullptr
				OV_ASSERT2(false);
				return false;
			}

			const void *key = event->data.ptr;

			if (key == this)
			{
				// A new client is connected to this socket
				DispatchAccept();
			}
			else
			{
				// An event raised by the client
				DispatchEvents(key, event);
			}
		}

		// Garbage collection
		{
			std::lock_guard<std::shared_mutex> lock(_client_list_mutex);
			_disconnected_client_list.clear();
		}

		return true;
	}

	std::shared_ptr<ClientSocket> ServerSocket::Accept()
	{
		SocketAddress address;
		SocketWrapper client_socket = Socket::Accept(&address);

		if (client_socket.IsValid() == false)
		{
			return nullptr;
		}

		std::shared_ptr<ClientSocket> client = std::make_shared<ClientSocket>(this, client_socket, address);

		if (client != nullptr)
		{
			logtd("[%p] [#%d] New client is connected: %s", this, _socket.GetSocket(), client->ToString().CStr());

			if (client->PrepareSocketOptions())
			{
				client->StartDispatchThread();

				_client_list_mutex.lock();
				_client_list[client.get()] = client;
				_client_list_mutex.unlock();

				AddToEpoll(client.get(), static_cast<void *>(client.get()));

				return client;
			}
			else
			{
				logte("[%p] [#%d] Could not prepare socket options: %s", this, _socket.GetSocket(), client->ToString().CStr());
			}
		}

		return nullptr;
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

			logtd("[%p] [#%d] Client #%d is connected", this, _socket.GetSocket(), client->GetSocket().GetSocket());

			auto new_state = _connection_callback(client, SocketConnectionState::Connected, nullptr);

			switch (new_state)
			{
				case SocketConnectionState::Connected:
					break;

				case SocketConnectionState::Disconnect:
					logtd("[%p] [#%d] The connection callback requested to disconnect the client #%d",
						  this, _socket.GetSocket(), client->GetSocket().GetSocket());
					DisconnectClient(client, new_state);
					break;

				case SocketConnectionState::Disconnected:
					logtd("[%p] [#%d] Invalid socket state for client #%d",
						  this, _socket.GetSocket(), client->GetSocket().GetSocket());
					OV_ASSERT2(false);
					DisconnectClient(client, new_state);
					break;

				case SocketConnectionState::Error: {
					auto error = Error::CreateError("Connection", "The connection callback requested to disconnect the client #%d",
													_socket.GetSocket(), client->GetSocket().GetSocket());
					logtd("[%p] [#%d] %s", this, _socket.GetSocket(), error->ToString().CStr());
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
				logtd("[%p] [#%d] Could not find a client: %p", this, _socket.GetSocket(), key);
				return;
			}

			client = item->second;
		}

		if (OV_CHECK_FLAG(epoll_events, EPOLLERR) || (!OV_CHECK_FLAG(epoll_events, EPOLLIN)))
		{
			// An error occurred while communiting with the client
			auto error = Error::CreateError("Epoll", "%s", StringFromEpollEvent(event).CStr());
			logte("[%p] [#%d] %s", this, _socket.GetSocket(), error->ToString().CStr());

			DisconnectClient(client, SocketConnectionState::Error, error);
		}
		else if (OV_CHECK_FLAG(event->events, EPOLLHUP) || OV_CHECK_FLAG(event->events, EPOLLRDHUP))
		{
			// The client is disconnected
			logtd("[%p] [#%d] Client #%d is disconnected with events: %s", this, _socket.GetSocket(), client->GetSocket().GetSocket(), StringFromEpollEvent(event).CStr());
			DisconnectClient(client, SocketConnectionState::Disconnected);
		}
		else
		{
			// Data is available that sent by the client
			auto data = std::make_shared<Data>(TcpBufferSize);

			logtd("[%p] [#%d] The data received from client #%d", this, _socket.GetSocket(), client->GetSocket().GetSocket());

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
							logtd("[%p] [#%d] The data callback requested to disconnect the client #%d",
								  this, _socket.GetSocket(), client->GetSocket().GetSocket());
							DisconnectClient(client, new_state);
							break;

						case SocketConnectionState::Disconnected:
							logtd("[%p] [#%d] Invalid socket state for client #%d",
								  this, _socket.GetSocket(), client->GetSocket().GetSocket());
							OV_ASSERT2(false);
							DisconnectClient(client, new_state);
							break;

						case SocketConnectionState::Error: {
							auto error = Error::CreateError("Connection", "The connection callback requested to disconnect the client #%d",
															_socket.GetSocket(), client->GetSocket().GetSocket());
							logtd("[%p] [#%d] %s", this, _socket.GetSocket(), error->ToString().CStr());
							DisconnectClient(client, new_state, error);
							break;
						}
					}
				}

				if (client->GetState() == SocketState::Error)
				{
					logtd("[%p] [#%d] An error occurred on client %s", this, _socket.GetSocket(), client->ToString().CStr());
					DisconnectClient(client, SocketConnectionState::Error, error);
					break;
				}

				if (error != nullptr)
				{
					logtd("[%p] [#%d] Client %s is disconnected", this, _socket.GetSocket(), client->ToString().CStr());
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

	bool ServerSocket::Close()
	{
		_client_list_mutex.lock();
		auto client_list = std::move(_client_list);
		_client_list_mutex.unlock();

		for (const auto &client : client_list)
		{
			client.second->Close();
		}

		return Socket::Close();
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
				logtd("[%p] [#%d] Deleting the client %s from list...", this, _socket.GetSocket(), client_socket->ToString().CStr());
				_disconnected_client_list[item->first] = item->second;
				_client_list.erase(item);

				remove = true;
			}
			else
			{
				logtd("[%p] [#%d] Could not find socket instance for %s", this, _socket.GetSocket(), client_socket->ToString().CStr());
			}
		}

		if (remove)
		{
			if (_connection_callback != nullptr)
			{
				_connection_callback(client_socket->GetSharedPtrAs<ClientSocket>(), state, error);
			}

			if (RemoveFromEpoll(client_socket.get()))
			{
				if (client_socket->GetState() != SocketState::Closed)
				{
					return client_socket->CloseInternal();
				}
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

	bool ServerSocket::SetSocketOptions(SocketType type, int send_buffer_size, int recv_buffer_size)
	{
		// SRT socket is already non-block mode
		bool result = true;

		if (type == SocketType::Tcp)
		{
			result &= SetSockOpt<int>(SO_REUSEADDR, 1);
			// result &= SetSockOpt<int>(IPPROTO_TCP, TCP_NODELAY, 1);

			int current_send_buffer_size;
			int current_recv_buffer_size;
			socklen_t data_size = sizeof(int);

			::getsockopt(_socket.GetSocket(), SOL_SOCKET, SO_SNDBUF, (void *)&current_send_buffer_size, &data_size);
			::getsockopt(_socket.GetSocket(), SOL_SOCKET, SO_RCVBUF, (void *)&current_recv_buffer_size, &data_size);

			// Setting send buffer size( 0 = default)
			if (send_buffer_size != 0 && current_send_buffer_size < send_buffer_size)
			{
				// setsockopt function SO_SNDBUF/SO_RCVBUF result is (input value *2)
				result &= SetSockOpt<int>(SO_SNDBUF, send_buffer_size / 2);

				::getsockopt(_socket.GetSocket(), SOL_SOCKET, SO_SNDBUF, (void *)&current_send_buffer_size, &data_size);

				logtd("TCP Server send buffer size (%u:%u)", current_send_buffer_size, send_buffer_size * 2);
			}

			// Setting recv buffer size( 0 = default)
			if (recv_buffer_size != 0 && current_recv_buffer_size < recv_buffer_size)
			{
				// setsockopt function SO_SNDBUF/SO_RCVBUF result is (input value *2)
				result &= SetSockOpt<int>(SO_RCVBUF, recv_buffer_size / 2);

				socklen_t data_size = sizeof(int);
				::getsockopt(_socket.GetSocket(), SOL_SOCKET, SO_RCVBUF, (void *)&current_recv_buffer_size, &data_size);

				logtd("TCP Server recv buffer size (%u:%u)", current_recv_buffer_size, recv_buffer_size * 2);
			}
		}
		else
		{
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
		}

		return result;
	}
}  // namespace ov
