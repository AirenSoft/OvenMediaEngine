//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "socket_address.h"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#include <memory>
#include <map>
#include <functional>

#include "../ovlibrary/ovlibrary.h"

namespace ov
{
	class Socket;

	// socket type
	typedef int socket_t;

	// socket()에서 실패했을때의 값
	const int InvalidSocket = -1;

	enum class SocketType : char
	{
		Unknown,
		Udp,
		Tcp,
	};

	enum class SocketState : char
	{
		Closed,
		Created,
		Bound,
		Listening,
		Connected,
		Error,
	};

	class Socket
	{
	public:
		Socket(SocketType type, socket_t socket, const sockaddr_in &remote_addr_in);
		// socket은 복사 불가능 (descriptor 까지 복사되는 것 방지)
		Socket(const Socket &socket) = delete;
		Socket(Socket &&socket) noexcept;
		virtual ~Socket();

		std::shared_ptr<SocketAddress> GetLocalAddress() const;
		std::shared_ptr<SocketAddress> GetRemoteAddress() const;

		// 현재 소켓의 접속 상태
		SocketState GetState() const;

		// 소켓 타입
		SocketType GetType() const;

		// 데이터 송신
		ssize_t Send(const void *data, ssize_t length);
		ssize_t Send(const std::shared_ptr<const Data> &data);

		ssize_t SendTo(const ov::SocketAddress &address, const void *data, ssize_t length);
		ssize_t SendTo(const ov::SocketAddress &address, const std::shared_ptr<const Data> &data);

		// 데이터 수신
		// 최대 ByteData의 capacity만큼 데이터를 기록
		// false가 반환되면 error를 체크해야 함
		std::shared_ptr<ov::Error> Recv(std::shared_ptr<Data> &data);

		// 최대 ByteData의 capacity만큼 데이터를 기록
		// nullptr이 반환되면 errno를 체크해야 함
		std::shared_ptr<ov::Error> RecvFrom(std::shared_ptr<Data> &data, std::shared_ptr<ov::SocketAddress> *address);

		// 소켓을 닫음
		virtual bool Close();

		virtual String ToString() const;

	protected:
		Socket();

		bool Create(SocketType type);
		bool MakeNonBlocking();

		bool Bind(const SocketAddress &address);

		bool Listen(int backlog = SOMAXCONN);

		template<typename T>
		std::shared_ptr<T> AcceptClient()
		{
			sockaddr_in client;

			socket_t client_socket = AcceptClientInternal(&client);

			if(client_socket != InvalidSocket)
			{
				// Accept()는 TCP에서만 일어남
				return std::make_shared<T>(SocketType::Tcp, client_socket, client);
			}

			return nullptr;
		}

		socket_t AcceptClientInternal(sockaddr_in *client);

		bool Connect(const SocketAddress &endpoint, int timeout = Infinite);

		bool PrepareEpoll();
		bool AddToEpoll(Socket *socket, void *parameter);
		int EpollWait(int timeout = Infinite);
		const epoll_event *EpollEvents(int index);
		bool RemoveFromEpoll(Socket *socket);

		// socket 옵션을 설정하는 함수
		template<class T>
		bool SetSockOpt(int option, const T &value)
		{
			return SetSockOpt(option, &value, (socklen_t)sizeof(T));
		}

		bool SetSockOpt(int option, const void *value, socklen_t value_length);

		void SetState(SocketState state);

		socket_t GetSocket() const
		{
			return _socket;
		}

		// utility method
		static String StringFromEpollEvent(const epoll_event *event);
		static String StringFromEpollEvent(const epoll_event &event);

		String ToString(const char *class_name) const;

	protected:
		socket_t _socket;

		// 소켓의 현재 상태
		SocketState _state;

		// 소켓 타입
		SocketType _type;
		// socket descriptor
		// socket_t _socket;

		// 로컬 정보
		std::shared_ptr<ov::SocketAddress> _local_address;
		// 리모트(peer) 정보
		std::shared_ptr<ov::SocketAddress> _remote_address;

		// nonblock 소켓 여부
		bool _is_nonblock;

		// epoll 관련
		socket_t _epoll;
		epoll_event *_epoll_events;
		int _last_epoll_event_count;
	};
}