//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "socket.h"
#include "socket_private.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

#include <base/ovlibrary/ovlibrary.h>
#include <errno.h>

namespace ov
{
	Socket::Socket()
		: _state(SocketState::Closed),

		  _type(SocketType::Tcp),
		  _socket(InvalidSocket),

		  _is_nonblock(false),

		  _epoll(InvalidSocket),
		  _epoll_events(nullptr),
		  _last_epoll_event_count(0)
	{
	}

	// remote 정보가 들어왔으므로, connect상태로 간주함
	Socket::Socket(SocketType type, socket_t socket, const sockaddr_in &remote_addr_in)
		: _state(SocketState::Connected),

		  _type(type),
		  _socket(socket),

		  _is_nonblock(false),

		  _epoll(InvalidSocket),
		  _epoll_events(nullptr),
		  _last_epoll_event_count(0)
	{
		// 로컬 정보는 없으며, 상대 정보만 있음. 즉, send만 가능
		_remote_address = std::make_shared<SocketAddress>(remote_addr_in);
	}

	Socket::Socket(Socket &&socket) noexcept
		: _state(socket._state),

		  _type(socket._type),

		  _socket(socket._socket),

		  _local_address(socket._local_address),
		  _remote_address(socket._remote_address),

		  _is_nonblock(socket._is_nonblock),

		  _epoll(socket._epoll),
		  _epoll_events(socket._epoll_events),
		  _last_epoll_event_count(socket._last_epoll_event_count)
	{
	}

	Socket::~Socket()
	{
		// _socket이 정상적으로 해제되었는지 확인
		OV_ASSERT(_socket == InvalidSocket, "Socket is not closed. Current state: %d", GetState());
		CHECK_STATE(== SocketState::Closed,);

		// epoll 관련 변수들이 정상적으로 해제되었는지 확인
		OV_ASSERT(_epoll == InvalidSocket, "Epoll is not uninitialized");
		OV_ASSERT(_epoll_events == nullptr, "Epoll events are not freed");
		OV_ASSERT(_last_epoll_event_count == 0, "Last epoll event count is remained");
	}

	bool Socket::Create(SocketType type)
	{
		CHECK_STATE(== SocketState::Closed, false);

		if(_socket != InvalidSocket)
		{
			logte("SocketBase is already created: %d", _socket);
			return false;
		}

		logtd("Creating new socket (type: %d)...", SocketType::Tcp);

		_socket = ::socket(PF_INET, (type == SocketType::Tcp) ? SOCK_STREAM : SOCK_DGRAM, 0);

		if(_socket == InvalidSocket)
		{
			logte("An error occurred while create socket");
			return false;
		}

		logtd("[%p] [#%d] SocketBase descriptor is created", this, _socket);

		SetState(SocketState::Created);
		_type = type;

		return true;
	}

	bool Socket::MakeNonBlocking()
	{
		int result;

		OV_ASSERT2(_socket != InvalidSocket);

		if(_socket == InvalidSocket)
		{
			logte("Could not make non blocking socket (Invalid socket)");
			return false;
		}

		result = ::fcntl(_socket, F_GETFL, 0);

		if(result == -1)
		{
			logte("Could not obtain flags from socket %d (%d)", _socket, result);
			return false;
		}

		int flags = result | O_NONBLOCK;

		result = ::fcntl(_socket, F_SETFL, flags);

		if(result == -1)
		{
			logte("Could not set flags to socket %d (%d)", _socket, result);
			return false;
		}

		_is_nonblock = true;
		return true;
	}

	bool Socket::Bind(const SocketAddress &address)
	{
		CHECK_STATE(== SocketState::Created, false);

		logtd("[%p] [#%d] Binding to %s...", this, _socket, address.ToString().CStr());

		int result = ::bind(_socket, address.Address(), (socklen_t)address.AddressLength());

		if(result == 0)
		{
			// 성공
			_local_address = std::make_shared<SocketAddress>(address);
		}
		else
		{
			// 실패
			logte("[%p] [#%d] Could not bind to %s (%d)", this, _socket, address.ToString().CStr(), result);
			return false;
		}

		SetState(SocketState::Bound);
		logtd("[%p] [#%d] Bound successfully", this, _socket);

		return true;
	}

	bool Socket::Listen(int backlog)
	{
		OV_ASSERT2(_type == SocketType::Tcp);
		CHECK_STATE(== SocketState::Bound, false);

		int result = ::listen(_socket, backlog);
		if(result == 0)
		{
			// 성공
			SetState(SocketState::Listening);
			return true;
		}

		logte("Could not listen: %s", Error::CreateErrorFromErrno()->ToString().CStr());
		return false;
	}

	socket_t Socket::AcceptClientInternal(sockaddr_in *client)
	{
		logtd("[%p] [#%d] New client is connected. Trying to accept the client...", this, _socket);

		OV_ASSERT2(_type == SocketType::Tcp);
		CHECK_STATE(<= SocketState::Listening, InvalidSocket);

		socklen_t client_length = sizeof(*client);

		socket_t client_socket = ::accept(_socket, reinterpret_cast<sockaddr *>(client), &client_length);

		OV_ASSERT2(client_length == sizeof(*client));

		return client_socket;
	}

	bool Socket::Connect(const SocketAddress &endpoint, int timeout)
	{
		OV_ASSERT2(_socket != InvalidSocket);
		CHECK_STATE(== SocketState::Created, false);

		// TODO: timeout 코드 넣기
		::connect(_socket, endpoint.Address(), endpoint.AddressLength());

		return true;
	}

	bool Socket::PrepareEpoll()
	{
		OV_ASSERT2(_epoll == InvalidSocket);

		if(_epoll != InvalidSocket)
		{
			logtw("[%p] [#%d] Epoll is already prepared", this, _socket);
			return false;
		}

		// epoll을 생성한 뒤,
		logtd("[%p] [#%d] Creating epoll...", this, _socket);

		_epoll = ::epoll_create1(0);

		if(_epoll == InvalidSocket)
		{
			logte("[%p] [#%d] Could not prepare epoll event: %s", this, _socket, Error::CreateErrorFromErrno()->ToString().CStr());
			return false;
		}

		return true;
	}

	bool Socket::AddToEpoll(Socket *socket, void *parameter)
	{
		CHECK_STATE(<= SocketState::Listening, false);

		OV_ASSERT2(_epoll != InvalidSocket);

		if(_epoll == InvalidSocket)
		{
			logte("[%p] [#%d] Invalid epoll descriptor: %d", this, _socket, _epoll);
			return false;
		}

		if(_epoll_events == nullptr)
		{
			_epoll_events = (epoll_event *)::calloc(EPOLL_MAX_EVENTS, sizeof(epoll_event));
			OV_ASSERT2(_epoll_events != nullptr);
		}

		epoll_event event;

		event.data.ptr = parameter;
		// EPOLLIN: input event 확인
		// EPOLLOUT: output event 확인
		// EPOLLERR: error event 확인
		// EPOLLHUP: hang up 확인
		// EPOLLPRI: 중요 데이터 확인
		// EPOLLET: ET 동작방식 설정
		event.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;

		logtd("[%p] [#%d] Trying to add socket #%d to epoll #%d...", this, _socket, socket->GetSocket(), _epoll);
		int result = ::epoll_ctl(_epoll, EPOLL_CTL_ADD, socket->GetSocket(), &event);

		if(result == -1)
		{
			logte("[%p] [#%d] Could not add to epoll for descriptor %d (error: %s)", this, _socket, socket->GetSocket(), Error::CreateErrorFromErrno()->ToString().CStr());
			return false;
		}

		return true;
	}

	int Socket::EpollWait(int timeout)
	{
		if(_epoll == InvalidSocket)
		{
			logte("[%p] [#%d] Epoll is not intialized", this, _socket);
			return -1;
		}

		int count = ::epoll_wait(_epoll, _epoll_events, EPOLL_MAX_EVENTS, timeout);

		if(count == -1)
		{
			// epoll_wait 호출 도중 오류 발생

			if(errno == EINTR)
			{
				// Interrupted system call - 앱을 종료하면 이 오류가 반환됨
				// TODO: 원래 그런것인가? close()를 잘 하면 괜찮을 것 같은데, 나중에 해결 방법을 찾자
			}
			else
			{
				// 기타 다른 오류 발생
				logte("[%p] [#%d] Could not wait for socket: %s", this, _socket, Error::CreateErrorFromErrno()->ToString().CStr());
			}

			_last_epoll_event_count = 0;
		}
		else if(count == 0)
		{
			// timed out
			// logtd("[%p] [#%d] epoll_wait() Timed out (timeout: %d ms)", this, _socket, timeout);
			_last_epoll_event_count = 0;
		}
		else if(count > 0)
		{
			// logtd("[%p] [#%d] %d events occurred", this, _socket, count);
			_last_epoll_event_count = count;
		}
		else
		{
			OV_ASSERT(false, "Unknown error");

			_last_epoll_event_count = 0;

			logte("[%p] [#%d] Could not wait for socket: %s (result: %d)", this, _socket, Error::CreateErrorFromErrno()->ToString().CStr(), count);
		}

		return count;
	}

	const epoll_event *Socket::EpollEvents(int index)
	{
		if(index >= _last_epoll_event_count)
		{
			return nullptr;
		}

		return &(_epoll_events[index]);
	}

	bool Socket::RemoveFromEpoll(Socket *socket)
	{
		CHECK_STATE(== SocketState::Listening, false);

		OV_ASSERT2(_epoll != InvalidSocket);

		if(_epoll == InvalidSocket)
		{
			logte("[%p] [#%d] Invalid epoll descriptor: %d", this, _socket, _epoll);
			return false;
		}

		logtd("[%p] [#%d] Trying to remove socket #%d from epoll...", this, _socket, socket->GetSocket());

		int result = ::epoll_ctl(_epoll, EPOLL_CTL_DEL, socket->GetSocket(), nullptr);

		if(result == -1)
		{
			logte("[%p] [#%d] Could not delete from epoll for descriptor %d (result: %d)", this, _socket, socket->GetSocket(), result);
			return false;
		}

		return true;
	}

	std::shared_ptr<ov::SocketAddress> Socket::GetLocalAddress() const
	{
		return _local_address;
	}

	std::shared_ptr<ov::SocketAddress> Socket::GetRemoteAddress() const
	{
		return _remote_address;
	}

	bool Socket::SetSockOpt(int option, const void *value, socklen_t value_length)
	{
		CHECK_STATE(!= SocketState::Closed, false);

		int result = ::setsockopt(_socket, SOL_SOCKET, option, value, value_length);

		if(result != 0)
		{
			logtw("[%p] [#%d] Could not set option: %d (result: %d)", this, _socket, option, result);
			return false;
		}

		return true;
	}

	SocketState Socket::GetState() const
	{
		return _state;
	}

	void Socket::SetState(SocketState state)
	{
		_state = state;
	}

	SocketType Socket::GetType() const
	{
		return _type;
	}

	ssize_t Socket::Send(const void *data, size_t length)
	{
		// TODO: 별도 send queue를 만들어야 함
		OV_ASSERT2(_socket != InvalidSocket);

		logtd("[%p] [#%d] Trying to send data:\n%s", this, _socket, ov::Dump(data, length, 64).CStr());

		auto data_to_send = static_cast<const uint8_t *>(data);
		size_t remained = length;
		size_t total_sent = 0L;

		while(remained > 0L)
		{
			ssize_t sent = ::send(_socket, data_to_send, remained, MSG_NOSIGNAL | (_is_nonblock ? MSG_DONTWAIT : 0));

			if(sent == -1L)
			{
				if(errno == EAGAIN)
				{
					continue;
				}

				logtw("[%p] [#%d] Could not send data: %zd", this, sent);

				break;
			}

			OV_ASSERT2(remained >= sent);

			remained -= sent;
			total_sent += sent;
			data_to_send += sent;
		}

		logtd("[%p] [#%d] Sent: %zu bytes", this, _socket, total_sent);

		return total_sent;
	}

	ssize_t Socket::Send(const std::shared_ptr<const Data> &data)
	{
		OV_ASSERT2(data != nullptr);

		return Send(data->GetData(), data->GetLength());
	}

	ssize_t Socket::SendTo(const ov::SocketAddress &address, const void *data, size_t length)
	{
		OV_ASSERT2(_socket != InvalidSocket);
		OV_ASSERT2(address.AddressForIPv4()->sin_addr.s_addr != 0);

		logtd("[%p] [#%d] Trying to send data %zu bytes to %s...", this, _socket, length, address.ToString().CStr());

		return ::sendto(_socket, data, length, MSG_NOSIGNAL | (_is_nonblock ? MSG_DONTWAIT : 0), address.Address(), address.AddressLength());
	}

	ssize_t Socket::SendTo(const ov::SocketAddress &address, const std::shared_ptr<const Data> &data)
	{
		OV_ASSERT2(data != nullptr);

		return SendTo(address, data->GetData(), data->GetLength());
	}

	std::shared_ptr<ov::Error> Socket::Recv(std::shared_ptr<Data> &data)
	{
		OV_ASSERT2(_socket != InvalidSocket);
		OV_ASSERT2(data != nullptr);
		OV_ASSERT2(data->GetCapacity() > 0);

		logtd("[%p] [#%d] Trying to read from the socket...", this, _socket);

		data->SetLength(data->GetCapacity());

		ssize_t read_bytes = ::recv(_socket, data->GetWritableData(), (size_t)data->GetLength(), (_is_nonblock ? MSG_DONTWAIT : 0));

		logtd("[%p] [#%d] Read bytes: %zd", this, _socket, read_bytes);

		if(read_bytes == 0L)
		{
			logtd("[%p] [#%d] Client is disconnected (errno: %d)", this, _socket, errno);

			data->SetLength(0L);
			Close();
		}
		else if(read_bytes < 0L)
		{
			auto error = Error::CreateErrorFromErrno();

			data->SetLength(0L);

			switch(error->GetCode())
			{
				case EAGAIN:
					// 클라이언트가 보낸 데이터를 끝까지 다 읽었음. 다음 데이터가 올 때까지 대기해야 함
					logtd("[%p] [#%d] There is no data to read", this, _socket);
					break;

				case ECONNRESET:
					// Connection reset by peer
					logtw("[%p] [#%d] Connection reset by peer", this, _socket);
					SetState(SocketState::Error);
					return error;

				default:
					logte("[%p] [#%d] An error occurred while read data: %s", this, _socket, error->ToString().CStr());
					SetState(SocketState::Error);
					return error;
			}
		}
		else
		{
			logtd("[%p] [#%d] %zd bytes read", this, _socket, read_bytes);

			data->SetLength(read_bytes);
		}

		return nullptr;
	}

	std::shared_ptr<ov::Error> Socket::RecvFrom(std::shared_ptr<Data> &data, std::shared_ptr<ov::SocketAddress> *address)
	{
		OV_ASSERT2(_socket != InvalidSocket);
		OV_ASSERT2(data != nullptr);
		OV_ASSERT2(data->GetCapacity() > 0);

		sockaddr_in remote = { 0 };
		socklen_t remote_length = sizeof(remote);

		logtd("[%p] [#%d] Trying to read from the socket...", this, _socket);
		data->SetLength(data->GetCapacity());

		ssize_t read_bytes = ::recvfrom(_socket, data->GetWritableData(), (size_t)data->GetLength(), (_is_nonblock ? MSG_DONTWAIT : 0), (sockaddr *)&remote, &remote_length);

		if(read_bytes < 0L)
		{
			auto error = Error::CreateErrorFromErrno();

			data->SetLength(0L);

			switch(error->GetCode())
			{
				case EAGAIN:
					// 클라이언트가 보낸 데이터를 끝까지 다 읽었음. 다음 데이터가 올 때까지 대기해야 함
					break;

				case ECONNRESET:
					// Connection reset by peer
					logtw("[%p] [#%d] Connection reset by peer", this, _socket);
					SetState(SocketState::Error);
					return error;

				default:
					logte("[%p] [#%d] An error occurred while read data: %s", this, _socket, error->ToString().CStr());
					SetState(SocketState::Error);
					return error;
			}
		}
		else
		{
			logtd("[%p] [#%d] %zd bytes read", this, _socket, read_bytes);

			data->SetLength(read_bytes);
			*address = std::make_shared<ov::SocketAddress>(remote);
		}

		return nullptr;
	}

	bool Socket::Close()
	{
		socket_t socket = _socket;

		if(_socket != InvalidSocket)
		{
			logtd("[%p] [#%d] Trying to close socket...", this, socket);

			CHECK_STATE(!= SocketState::Closed, false);

			if(_type == SocketType::Tcp)
			{
				if(_socket != InvalidSocket)
				{
					// FIN 송신
					::shutdown(_socket, SHUT_WR);
				}
			}

			// socket 관련
			OV_SAFE_FUNC(_socket, InvalidSocket, ::close,);

			// epoll 관련
			OV_SAFE_FUNC(_epoll, InvalidSocket, ::close,);
			OV_SAFE_FREE(_epoll_events);

			logtd("[%p] [#%d] SocketBase is closed successfully", this, socket);

			SetState(SocketState::Closed);

			return true;
		}

		logtd("[%p] Socket is already closed");

		OV_ASSERT2(_state == SocketState::Closed);

		return false;
	}

	String Socket::StringFromEpollEvent(const epoll_event *event)
	{
		return StringFromEpollEvent(*event);
	}

	String Socket::StringFromEpollEvent(const epoll_event &event)
	{
		std::vector<String> flags;

		ADD_FLAG_IF(flags, event.events, EPOLLIN);
		ADD_FLAG_IF(flags, event.events, EPOLLPRI);
		ADD_FLAG_IF(flags, event.events, EPOLLOUT);
		ADD_FLAG_IF(flags, event.events, EPOLLRDNORM);
		ADD_FLAG_IF(flags, event.events, EPOLLRDBAND);
		ADD_FLAG_IF(flags, event.events, EPOLLWRNORM);
		ADD_FLAG_IF(flags, event.events, EPOLLWRBAND);
		ADD_FLAG_IF(flags, event.events, EPOLLMSG);
		ADD_FLAG_IF(flags, event.events, EPOLLERR);
		ADD_FLAG_IF(flags, event.events, EPOLLHUP);
		ADD_FLAG_IF(flags, event.events, EPOLLRDHUP);
		ADD_FLAG_IF(flags, event.events, EPOLLWAKEUP);
		ADD_FLAG_IF(flags, event.events, EPOLLONESHOT);
		ADD_FLAG_IF(flags, event.events, EPOLLET);

		return ov::String::Join(flags, " | ");
	}

	String Socket::ToString(const char *class_name) const
	{
		if(_socket == InvalidSocket)
		{
			return String::FormatString("<%s: %p, state: %d>", class_name, this, _state);
		}
		else
		{
			return String::FormatString("<%s: %p, (%s) #%d, state: %d>", class_name, this, (_type == SocketType::Tcp) ? "TCP" : "UDP", _socket, _state);
		}
	}

	String Socket::ToString() const
	{
		return ToString("Socket");
	}
}
