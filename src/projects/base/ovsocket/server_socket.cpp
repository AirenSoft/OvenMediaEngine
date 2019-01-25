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
				// SRT socket is already non-block mode
				((type == SocketType::Tcp) ? SetSockOpt<int>(SO_REUSEADDR, 1) : SetSockOpt(SRTO_REUSEADDR, true)) &&
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
                    connection_callback(client_socket, SocketConnectionState::Disconnected);

				// client map에서 삭제
				need_to_delete = true;
			}
            else if(OV_CHECK_FLAG(event->events, EPOLLHUP) || OV_CHECK_FLAG(event->events, EPOLLRDHUP))
            {
                logti("[%p] [#%d] Client is disconnected - events(%s)", this, _socket.GetSocket(), StringFromEpollEvent(event).CStr());

                if(client_socket != nullptr)
                    connection_callback(client_socket, SocketConnectionState::Disconnected);

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

	bool ServerSocket::RemoveClientSocket(ClientSocket * client_socket)
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
}
