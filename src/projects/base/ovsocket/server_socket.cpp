//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "server_socket.h"
#include "socket_private.h"
#include "client_socket.h"

namespace ov
{
	bool ServerSocket::Prepare(SocketType type, uint16_t port, int backlog)
	{
		return Prepare(type, SocketAddress(port), backlog);
	}

	bool ServerSocket::Prepare(SocketType type, const SocketAddress &address, int backlog)
	{
		CHECK_STATE(== SocketState::Closed, false);

		if(
			(
				Create(type) &&
				MakeNonBlocking() &&
				PrepareEpoll() &&
				AddToEpoll(this, static_cast<void *>(this)) &&
				SetSocketOptions(type) &&
				Bind(address) &&
				Listen(backlog)
			) == false)
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

		int count = EpollWait(timeout);

		for(int index = 0; index < count; index++)
		{
			const epoll_event *event = EpollEvents(index);

			if(event == nullptr)
			{
				// 버그가 아닌 이상 nullptr이 될 수 없음
				OV_ASSERT2(false);
				return false;
			}

			void *epoll_data = event->data.ptr;
			ClientSocket *client_socket = (epoll_data == (void *)this) ? nullptr : static_cast<ClientSocket *>(epoll_data);

			bool need_to_delete = false;

			if(
				OV_CHECK_FLAG(event->events, EPOLLERR) ||
				(!OV_CHECK_FLAG(event->events, EPOLLIN))
				)
			{
				// 오류 발생
				logte("[%p] [#%d] Epoll error: %p, events: %s (%d)", this, _socket.GetSocket(), epoll_data, StringFromEpollEvent(event).CStr(), event->events);

				if(client_socket != nullptr && (OV_CHECK_FLAG(event->events, EPOLLHUP) || OV_CHECK_FLAG(event->events, EPOLLRDHUP)))
				{
					connection_callback(client_socket, SocketConnectionState::Disconnected);
				}

				// client map에서 삭제
				need_to_delete = true;
			}
			else if(OV_CHECK_FLAG(event->events, EPOLLHUP) || OV_CHECK_FLAG(event->events, EPOLLRDHUP))
			{
				logtd("[%p] [#%d] Client is disconnected - events(%s)", this, _socket.GetSocket(), StringFromEpollEvent(event).CStr());

				if(client_socket != nullptr)
				{
					connection_callback(client_socket, SocketConnectionState::Disconnected);
				}

				// client map에서 삭제
				need_to_delete = true;
			}
			else if(epoll_data == static_cast<void *>(this))
			{
				// listen된 socket에서 이벤트 발생

				// listen된 socket에 새로운 클라이언트가 접속함
				int accept_count = 0;
				std::shared_ptr<ClientSocket> client = nullptr;

				do
				{
					client = Accept();

					if(client != nullptr)
					{
						logtd("[%p] [#%d] Client is connected - events(%s)", this, _socket.GetSocket(), StringFromEpollEvent(event).CStr());

						// 정상적으로 accept 되었다면 callback
						need_to_delete = connection_callback(client.get(), SocketConnectionState::Connected);

						// callback의 반환 값이 true면(need_to_delete = true), 아래에서 client 없앰

						accept_count++;
					}
				} while(client != nullptr);

				if(accept_count <= 0 && client_socket != nullptr)
				{
					// accept 실패. 아래에서 client 없앰 (정상적인 상황이라면, map 자체에 등록되어 있지 않음)
					need_to_delete = true;
				}
			}
			else
			{
				// client socket에서 데이터를 읽을 준비가 됨

				while(true)
				{
					auto data = std::make_shared<Data>(TcpBufferSize);

					auto error = client_socket->Recv(data);

					if(data->GetLength() > 0L)
					{
						need_to_delete = data_callback(client_socket, data);
						// socket이 error 상태가 되었다면 삭제
						need_to_delete = need_to_delete || (client_socket->GetState() == SocketState::Error);
					}

					if(client_socket->GetState() == SocketState::Closed)
					{
						logtd("[%p] [#%d] Client %s is disconnected (Socket is closed)", this, _socket.GetSocket(), client_socket->ToString().CStr());
						connection_callback(client_socket, SocketConnectionState::Disconnected);
						break;
					}
					else if(client_socket->GetState() == SocketState::Error)
					{
						logtd("[%p] [#%d] An error occurred on client %s", this, _socket.GetSocket(), client_socket->ToString().CStr());
						connection_callback(client_socket, SocketConnectionState::Error);
						break;
					}

					if(error != nullptr)
					{
						logtd("[%p] [#%d] Client %s is disconnected", this, _socket.GetSocket(), client_socket->ToString().CStr());
						need_to_delete = true;
						break;
					}
					else
					{
						if(data->GetLength() == 0L)
						{
							// 다음 데이터를 기다려야 함
							break;
						}
					}
				}
			}

			if(need_to_delete)
			{
				RemoveClientSocket(client_socket);
			}
		}

		return true;
	}

	std::shared_ptr<ClientSocket> ServerSocket::Accept()
	{
		CHECK_STATE(== SocketState::Listening, nullptr);

		std::shared_ptr<ClientSocket> client = Socket::AcceptClient<ClientSocket>();

		if(client != nullptr)
		{
			logtd("[%p] [#%d] New client is connected: %s", this, _socket.GetSocket(), client->ToString().CStr());


			std::unique_lock<std::mutex> lock(_client_list_mutex);

			// 외부 Thread 접근 으로 동기화처리 필요
			_client_list[client.get()] = client;

			AddToEpoll(client.get(), static_cast<void *>(client.get()));

			return client;
		}

		return nullptr;
	}

	bool ServerSocket::Close()
	{
		// TODO: client list 정리

		// std::map<ClientSocket *, std::shared_ptr<ClientSocket>> _client_list;

		return Socket::Close();
	}

	String ServerSocket::ToString() const
	{
		return Socket::ToString("ServerSocket");
	}

	bool ServerSocket::RemoveClientSocket(ClientSocket *client_socket)
	{
		// 리소스 해제
		if(client_socket == nullptr)
		{
			return false;
		}

		RemoveFromEpoll(client_socket);

		if(client_socket->GetState() != SocketState::Closed)
		{
			client_socket->Close();
		}

		// 외부 Thread 접근 으로 동기화처리 필요
		std::unique_lock<std::mutex> lock(_client_list_mutex);

		// client list 에서도 해당 항목을 삭제 해야 함
		auto item = _client_list.find(client_socket);

		if(item != _client_list.end())
		{
			_client_list.erase(item);
		}
		else
		{
			logtd("[%d] Could not find socket instance for %s", _socket.GetSocket(), client_socket->ToString().CStr());
		}


		return true;
	}

	bool ServerSocket::SetSocketOptions(SocketType type)
	{
		// SRT socket is already non-block mode
		bool result = true;

		if(type == SocketType::Tcp)
		{
			result &= SetSockOpt<int>(SO_REUSEADDR, 1);
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
}
