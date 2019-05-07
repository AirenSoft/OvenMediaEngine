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
#include <algorithm>

#include <base/ovlibrary/ovlibrary.h>
#include <errno.h>

#include <chrono>
#include <atomic>

#define USE_STATS_COUNTER                       0

namespace ov
{
#if USE_STATS_COUNTER
	class StatsCounter
	{
	public:
		void IncreasePps()
		{
			_count++;
			_total_count++;
		}

		void IncreaseRetry()
		{
			_retry_count++;
			_total_retry_count++;
		}

		void IncreaseError()
		{
			_error_count++;
			_total_error_count++;
		}

		void StartTracking()
		{
			{
				std::lock_guard<std::mutex> lock_guard(_mutex);
				if(_is_running)
				{
					return;
				}

				_is_running = true;
			}

			_stop = false;

			_tracking_thread = std::thread(
				[this]()
				{
					int64_t min = INT64_MAX;
					int64_t max = INT64_MIN;

					int64_t retry_min = INT64_MAX;
					int64_t retry_max = INT64_MIN;

					int64_t error_min = INT64_MAX;
					int64_t error_max = INT64_MIN;

					int64_t loop_count = 0;
					while(_stop == false)
					{
						int64_t count = _count;
						_count = 0;

						int64_t retry_count = _retry_count;
						_retry_count = 0;

						int64_t error_count = _error_count;
						_error_count = 0;

						if((count > 0) || (retry_count) || (error_count > 0))
						{
							loop_count++;

							min = std::min(min, count);
							max = std::max(max, count);

							retry_min = std::min(retry_min, retry_count);
							retry_max = std::max(retry_max, retry_count);

							error_min = std::min(error_min, error_count);
							error_max = std::max(error_max, error_count);

							int64_t average = _total_count / ((loop_count == 0) ? 1 : loop_count);
							int64_t retry_average = _total_retry_count / ((loop_count == 0) ? 1 : loop_count);
							int64_t error_average = _total_error_count / ((loop_count == 0) ? 1 : loop_count);

							logi("SockStat",
								 "[Stats Counter] Total sampling count: %ld\n"
								 "+-------+---------+---------+---------+---------+--------------+\n"
								 "| Type  | Current |   Max   |   Min   | Average |    Total     |\n"
								 "+-------+---------+---------+---------+---------+--------------+\n"
								 "| PPS   | %7ld | %7ld | %7ld | %7ld | %12ld |\n"
								 "| Retry | %7ld | %7ld | %7ld | %7ld | %12ld |\n"
								 "| Error | %7ld | %7ld | %7ld | %7ld | %12ld |\n"
								 "+-------+---------+---------+---------+---------+--------------+\n",
								 loop_count,
								 count, max, min, average, static_cast<int64_t>(_total_count),
								 retry_count, retry_max, retry_min, retry_average, static_cast<int64_t>(_total_retry_count),
								 error_count, error_max, error_min, error_average, static_cast<int64_t>(_total_error_count)
							);
						}

						sleep(1);
					}
				});
		}

		void StopTracking()
		{
			_stop = true;
		}

	protected:
		std::mutex _mutex;
		volatile bool _is_running = false;

		std::atomic<int64_t> _count { 0 };
		std::atomic<int64_t> _total_count { 0 };

		std::atomic<int64_t> _retry_count { 0 };
		std::atomic<int64_t> _total_retry_count { 0 };

		std::atomic<int64_t> _error_count { 0 };
		std::atomic<int64_t> _total_error_count { 0 };

		std::thread _tracking_thread;
		volatile bool _stop = true;
	};

	static StatsCounter stats_counter;
#endif // USE_STATS_COUNTER

	Socket::Socket()
	{
#if USE_STATS_COUNTER
		stats_counter.StartTracking();
#endif // USE_STATS_COUNTER
	}

	// remote 정보가 들어왔으므로, connect상태로 간주함
	Socket::Socket(SocketWrapper socket, const SocketAddress &remote_address)
		: _socket(socket),

		  _state(SocketState::Connected)
	{
		// 로컬 정보는 없으며, 상대 정보만 있음. 즉, send만 가능
		_remote_address = std::make_shared<SocketAddress>(remote_address);
	}

	Socket::Socket(Socket &&socket) noexcept
		: _socket(socket._socket),

		  _state(socket._state),

		  _local_address(std::move(socket._local_address)),
		  _remote_address(std::move(socket._remote_address)),

		  _is_nonblock(socket._is_nonblock),

		  _epoll(socket._epoll),
		  _epoll_events(socket._epoll_events),
		  _last_epoll_event_count(socket._last_epoll_event_count)
	{
	}

	Socket::~Socket()
	{
		// _socket이 정상적으로 해제되었는지 확인
		OV_ASSERT(_socket.IsValid() == false, "Socket is not closed. Current state: %d", GetState());
		CHECK_STATE(== SocketState::Closed,);

		// epoll 관련 변수들이 정상적으로 해제되었는지 확인
		OV_ASSERT(_epoll == InvalidSocket, "Epoll is not uninitialized");
		OV_ASSERT(_epoll_events == nullptr, "Epoll events are not freed");

		// TODO(dimiden): PhysicalPort에서 이벤트를 모두 처리하지 않고 Socket을 바로 Close()하는 부분이 있는데,
		// 나중에 half-close를 한 뒤, 나머지 이벤트들을 모두 처리하고 나서 최종적으로 Close()하도록 해야함
		// OV_ASSERT(_last_epoll_event_count == 0, "Last epoll event count is remained: %d", _last_epoll_event_count);
	}

	bool Socket::Create(SocketType type)
	{
		CHECK_STATE(== SocketState::Closed, false);

		if(_socket.IsValid())
		{
			logte("SocketBase is already created: %d", _socket.GetSocket());
			return false;
		}

		logtd("Trying to create new socket (type: %d)...", type);

		switch(type)
		{
			case SocketType::Tcp:
			case SocketType::Udp:
				_socket.SetSocket(type, ::socket(PF_INET, (type == SocketType::Tcp) ? SOCK_STREAM : SOCK_DGRAM, 0));
				break;

			case SocketType::Srt:
				_socket.SetSocket(type, ::srt_socket(AF_INET, SOCK_DGRAM, 0));
				break;

			default:
				break;
		}

		if(_socket.IsValid() == false)
		{
			logte("An error occurred while create socket");
			return false;
		}

		logtd("[%p] [#%d] SocketBase descriptor is created for type %d", this, _socket.GetSocket(), type);

		SetState(SocketState::Created);

		return true;
	}

	bool Socket::MakeNonBlocking()
	{
		int result;

		if(_socket.IsValid() == false)
		{
			logte("Could not make non blocking socket (Invalid socket)");
			OV_ASSERT2(_socket.IsValid());
			return false;
		}

		switch(GetType())
		{
			case SocketType::Tcp:
			case SocketType::Udp:
			{
				result = ::fcntl(_socket.GetSocket(), F_GETFL, 0);

				if(result == -1)
				{
					logte("Could not obtain flags from socket %d (%d)", _socket.GetSocket(), result);
					return false;
				}

				int flags = result | O_NONBLOCK;

				result = ::fcntl(_socket.GetSocket(), F_SETFL, flags);

				if(result == -1)
				{
					logte("Could not set flags to socket %d (%d)", _socket.GetSocket(), result);
					return false;
				}

				_is_nonblock = true;
				return true;
			}

			case SocketType::Srt:
				if(SetSockOpt(SRTO_RCVSYN, false) && SetSockOpt(SRTO_SNDSYN, false))
				{
					_is_nonblock = true;
				}

				return _is_nonblock;

			default:
				OV_ASSERT(false, "Invalid socket type: %d", GetType());
				break;
		}

		return false;
	}

	bool Socket::Bind(const SocketAddress &address)
	{
		CHECK_STATE(== SocketState::Created, false);

		if(_socket.IsValid() == false)
		{
			logte("Could not bind socket (Invalid socket)");
			OV_ASSERT2(_socket.IsValid());
			return false;
		}

		logtd("[%p] [#%d] Binding to %s...", this, _socket.GetSocket(), address.ToString().CStr());

		switch(GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
			{
				int result = ::bind(_socket.GetSocket(), address.Address(), static_cast<socklen_t>(address.AddressLength()));

				if(result == 0)
				{
					// 성공
					_local_address = std::make_shared<SocketAddress>(address);
				}
				else
				{
					// 실패
					logte("[%p] [#%d] Could not bind to %s (%d)", this, _socket.GetSocket(), address.ToString().CStr(), result);
					return false;
				}

				break;
			}

			case SocketType::Srt:
			{
				int result = ::srt_bind(_socket.GetSocket(), address.Address(), static_cast<int>(address.AddressLength()));

				if(result != SRT_ERROR)
				{
					// 성공
					_local_address = std::make_shared<SocketAddress>(address);
				}
				else
				{
					// 실패
					logte("[%p] [#%d] Could not bind to %s for SRT (%s)", this, _socket.GetSocket(), address.ToString().CStr(), srt_getlasterror_str());
					return false;
				}

				break;
			}

			default:
				OV_ASSERT(false, "Invalid socket type: %d", GetType());
				return false;
		}

		SetState(SocketState::Bound);
		logtd("[%p] [#%d] Bound successfully", this, _socket.GetSocket());

		return true;
	}

	bool Socket::Listen(int backlog)
	{
		CHECK_STATE(== SocketState::Bound, false);

		switch(GetType())
		{
			case SocketType::Tcp:
			{
				int result = ::listen(_socket.GetSocket(), backlog);
				if(result == 0)
				{
					// 성공
					SetState(SocketState::Listening);
					return true;
				}

				logte("Could not listen: %s", Error::CreateErrorFromErrno()->ToString().CStr());
				break;
			}

			case SocketType::Srt:
			{
				int result = ::srt_listen(_socket.GetSocket(), backlog);
				if(result != SRT_ERROR)
				{
					SetState(SocketState::Listening);
					return true;
				}

				logte("Could not listen: %s", srt_getlasterror_str());
				break;
			}

			default:
				OV_ASSERT(false, "Invalid socket type: %d", GetType());
				break;
		}

		return false;
	}

	SocketWrapper Socket::AcceptClientInternal(SocketAddress *client)
	{
		logtd("[%p] [#%d] New client is connected. Trying to accept the client...", this, _socket.GetSocket());

		CHECK_STATE(<= SocketState::Listening, SocketWrapper());

		switch(GetType())
		{
			case SocketType::Tcp:
			{
				sockaddr_in client_addr {};
				socklen_t client_length = sizeof(client_addr);

				socket_t client_socket = ::accept(_socket.GetSocket(), reinterpret_cast<sockaddr *>(&client_addr), &client_length);

				if(client_socket != InvalidSocket)
				{
					*client = SocketAddress(client_addr);
				}

				return SocketWrapper(GetType(), client_socket);
			}

			case SocketType::Srt:
			{
				sockaddr_storage client_addr {};
				int client_length = sizeof(client_addr);

				SRTSOCKET client_socket = ::srt_accept(_socket.GetSocket(), reinterpret_cast<sockaddr *>(&client_addr), &client_length);

				if(client_socket != SRT_INVALID_SOCK)
				{
					*client = SocketAddress(client_addr);
				}

				return SocketWrapper(GetType(), client_socket);
			}

			default:
				OV_ASSERT(false, "Invalid socket type: %d", GetType());
				break;
		}

		return SocketWrapper();
	}

	std::shared_ptr<ov::Error> Socket::Connect(const SocketAddress &endpoint, int timeout)
	{
		OV_ASSERT2(_socket.IsValid());
		CHECK_STATE(== SocketState::Created, ov::Error::CreateError(EINVAL, "Invalid state: %d", static_cast<int>(_state)));

		std::shared_ptr<ov::Error> error;

		// TODO: timeout 코드 넣기
		switch(GetType())
		{
			case SocketType::Tcp:
			case SocketType::Udp:
				if(::connect(_socket.GetSocket(), endpoint.Address(), endpoint.AddressLength()) == 0)
				{
					return nullptr;
				}

				error = ov::Error::CreateErrorFromErrno();

				break;

			case SocketType::Srt:

				if(SetSockOpt(SRTO_CONNTIMEO, timeout) == false)
				{
					error = ov::Error::CreateErrorFromSrt();
				}
				else if(::srt_connect(_socket.GetSocket(), endpoint.Address(), endpoint.AddressLength()) != SRT_ERROR)
				{
					return nullptr;
				}

				error = ov::Error::CreateErrorFromSrt();

				break;

			default:
				break;
		}

		Close();

		return error;
	}

	bool Socket::PrepareEpoll()
	{
		switch(GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				if(_epoll != InvalidSocket)
				{
					logtw("[%p] [#%d] Epoll is already prepared: %d", this, _socket.GetSocket(), _epoll);
					OV_ASSERT2(_epoll == InvalidSocket);
					return false;
				}

				logtd("[%p] [#%d] Creating epoll...", this, _socket.GetSocket());

				_epoll = ::epoll_create1(0);

				if(_epoll != InvalidSocket)
				{
					return true;
				}

				logte("[%p] [#%d] Could not prepare epoll event: %s", this, _socket.GetSocket(), Error::CreateErrorFromErrno()->ToString().CStr());

				break;

			case SocketType::Srt:
				if(_srt_epoll != SRT_INVALID_SOCK)
				{
					logtw("[%p] [#%d] SRT Epoll is already prepared: %d", this, _socket.GetSocket(), _srt_epoll);
					OV_ASSERT2(_srt_epoll == SRT_INVALID_SOCK);
					return false;
				}

				logtd("[%p] [#%d] Creating epoll for SRT...", this, _socket.GetSocket());

				_srt_epoll = ::srt_epoll_create();

				if(_srt_epoll != SRT_INVALID_SOCK)
				{
					return true;
				}

				logte("[%p] [#%d] Could not prepare epoll event for SRT: %s", this, _socket.GetSocket(), srt_getlasterror_str());

				break;

			default:
				break;
		}

		return false;
	}

	bool Socket::AddToEpoll(Socket *socket, void *parameter)
	{
		CHECK_STATE(<= SocketState::Listening, false);

		if(_epoll_events == nullptr)
		{
			_epoll_events = (epoll_event *)::calloc(EpollMaxEvents, sizeof(epoll_event));
			OV_ASSERT2(_epoll_events != nullptr);
		}

		switch(GetType())
		{
			case SocketType::Tcp:
			case SocketType::Udp:
			{
				if(_epoll != InvalidSocket)
				{
					epoll_event event {};

					event.data.ptr = parameter;
					// EPOLLIN: input event 확인
					// EPOLLOUT: output event 확인
					// EPOLLERR: error event 확인
					// EPOLLHUP: hang up 확인
					// EPOLLPRI: 중요 데이터 확인
					// EPOLLET: ET 동작방식 설정
					// EPOLLRDHUP : 연결이 종료되거나 Half-close 가 진행된 상황
					event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;

					logtd("[%p] [#%d] Trying to add socket #%d to epoll #%d...", this, _socket.GetSocket(), socket->_socket.GetSocket(), _epoll);
					int result = ::epoll_ctl(_epoll, EPOLL_CTL_ADD, socket->_socket.GetSocket(), &event);

					if(result != -1)
					{
						return true;
					}

					logte("[%p] [#%d] Could not add to epoll for descriptor %d (error: %s)", this, _socket.GetSocket(), socket->_socket.GetSocket(), Error::CreateErrorFromErrno()->ToString().CStr());
				}
				else
				{
					logte("[%p] [#%d] Invalid epoll descriptor: %d", this, _socket.GetSocket(), _epoll);
					OV_ASSERT2(_epoll != InvalidSocket);
				}

				break;
			}

			case SocketType::Srt:
			{
				if(_srt_epoll != SRT_INVALID_SOCK)
				{
					int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;

					int result = ::srt_epoll_add_usock(_srt_epoll, socket->_socket.GetSocket(), &events);

					if(result != SRT_ERROR)
					{
						_srt_parameter_map[socket->_socket.GetSocket()] = parameter;
						return true;
					}

					logte("[%p] [#%d] Could not add to epoll for descriptor %d (error: %s)", this, _socket.GetSocket(), socket->_socket.GetSocket(), srt_getlasterror_str());
				}
				else
				{
					logte("[%p] [#%d] Invalid epoll descriptor: %d", this, _socket.GetSocket(), _srt_epoll);
					OV_ASSERT2(_srt_epoll != SRT_INVALID_SOCK);
				}

				break;
			}

			default:
				break;
		}

		return false;
	}

	int Socket::EpollWait(int timeout)
	{
		switch(GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
			{
				if(_epoll == InvalidSocket)
				{
					logte("[%p] [#%d] Epoll is not intialized", this, _socket.GetSocket());
					return -1;
				}

				int count = ::epoll_wait(_epoll, _epoll_events, EpollMaxEvents, timeout);

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
						logte("[%p] [#%d] Could not wait for socket: %s", this, _socket.GetSocket(), Error::CreateErrorFromErrno()->ToString().CStr());
					}

					_last_epoll_event_count = 0;
				}
				else if(count == 0)
				{
					// timed out
					// logtd("[%p] [#%d] epoll_wait() Timed out (timeout: %d ms)", this, _socket.GetSocket(), timeout);
					_last_epoll_event_count = 0;
				}
				else if(count > 0)
				{
					// logtd("[%p] [#%d] %d events occurred", this, _socket.GetSocket(), count);
					_last_epoll_event_count = count;
				}
				else
				{
					OV_ASSERT(false, "Unknown error");

					_last_epoll_event_count = 0;

					logte("[%p] [#%d] Could not wait for socket: %s (result: %d)", this, _socket.GetSocket(), Error::CreateErrorFromErrno()->ToString().CStr(), count);
				}

				return count;
			}

			case SocketType::Srt:
			{
				if(_srt_epoll == SRT_INVALID_SOCK)
				{
					logte("[%p] [#%d] Epoll is not intialized", this, _socket.GetSocket());
					return -1;
				}

				int count = EpollMaxEvents;
				SRTSOCKET read_list[EpollMaxEvents];

				int result = ::srt_epoll_wait(_srt_epoll, read_list, &count, nullptr, nullptr, timeout, nullptr, nullptr, nullptr, nullptr);

				if(result > 0)
				{
					if(count == 0)
					{
#if DEBUG
						int srt_lasterror = srt_getlasterror(nullptr);
						OV_ASSERT((srt_lasterror == SRT_ETIMEOUT), "Not handled last error: %d", srt_lasterror);
#endif // DEBUG
						_last_epoll_event_count = 0;
					}
					else if(count > 0)
					{
						logtd("[%p] [#%d] %d events occurred", this, _socket.GetSocket(), count);
						_last_epoll_event_count = count;

						for(int index = 0; index < count; index++)
						{
							SRTSOCKET sock = read_list[index];
							SRT_SOCKSTATUS status = srt_getsockstate(sock);

							// Make epoll_event from SRT socket
							epoll_event *event = &(_epoll_events[index]);

							event->data.ptr = _srt_parameter_map[sock];
							event->events = EPOLLIN;

							switch(status)
							{
								case SRTS_LISTENING:
									// New SRT client connection
									break;

								case SRTS_NONEXIST:
									event->events |= EPOLLHUP;
									break;

								case SRTS_BROKEN:
									// The client is disconnected (unexpected)
									event->events |= EPOLLHUP;
									break;

								case SRTS_CLOSED:
									// The client is disconnected (expected)
									event->events |= EPOLLHUP;
									break;

								case SRTS_CONNECTED:
									// A client is connected
									break;

								default:
									logtd("[%p] [#%d] %d status: %d", this, _socket.GetSocket(), sock, status);
									break;
							}
						}
					}
				}
				else if(result == 0)
				{
					logte("[%p] [#%d] Could not wait for socket: %s", this, _socket.GetSocket(), srt_getlasterror_str());

					_last_epoll_event_count = 0;
				}
				else
				{
					// epoll_wait 호출 도중 오류 발생

					if(srt_getlasterror(nullptr) == SRT_ETIMEOUT)
					{
						// timed out
						//logtd("[%p] [#%d] epoll_wait() Timed out (timeout: %d ms)", this, _socket.GetSocket(), timeout);
					}
					else
					{
						logte("[%p] [#%d] Could not wait for socket: %s", this, _socket.GetSocket(), srt_getlasterror_str());
					}

					_last_epoll_event_count = 0;
				}

				return _last_epoll_event_count;
			}

			case SocketType::Unknown:
				logte("[%p] [#%d] Unknown socket type", this, _socket.GetSocket());
				break;
		}

		return -1;
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

		switch(GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
			{
				if(_epoll == InvalidSocket)
				{
					logte("[%p] [#%d] Invalid epoll descriptor: %d", this, _socket.GetSocket(), _epoll);
					OV_ASSERT2(_epoll != InvalidSocket);
					return false;
				}

				logtd("[%p] [#%d] Trying to remove a socket #%d from epoll...", this, _socket.GetSocket(), socket->_socket.GetSocket());

				int result = ::epoll_ctl(_epoll, EPOLL_CTL_DEL, socket->_socket.GetSocket(), nullptr);

				if(result == -1)
				{
					logte("[%p] [#%d] Could not delete the socket from epoll for descriptor %d (result: %s)", this, _socket.GetSocket(), socket->_socket.GetSocket(), ov::Error::CreateErrorFromErrno()->ToString().CStr());
					logte("\n%s", ov::StackTrace::GetStackTrace().CStr());
					return false;
				}

				break;
			}

			case SocketType::Srt:
			{
				if(_srt_epoll == SRT_INVALID_SOCK)
				{
					logte("[%p] [#%d] Invalid epoll descriptor: %d", this, _socket.GetSocket(), _srt_epoll);
					OV_ASSERT2(_srt_epoll != SRT_INVALID_SOCK);
					return false;
				}

				SRTSOCKET sock = socket->_socket.GetSocket();

				logtd("[%p] [#%d] Trying to remove a SRT socket #%d from epoll...", this, _socket.GetSocket(), sock);

				int result = ::srt_epoll_remove_usock(_srt_epoll, sock);

				_srt_parameter_map.erase(sock);

				if(result == SRT_ERROR)
				{
					logte("[%p] [#%d] Could not delete the SRT socket from epoll for descriptor %d (result: %s)", this, _socket.GetSocket(), sock, ov::Error::CreateErrorFromSrt()->ToString().CStr());
					return false;
				}

				break;
			}

			default:
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

		int result = ::setsockopt(_socket.GetSocket(), SOL_SOCKET, option, value, value_length);

		if(result != 0)
		{
			logtw("[%p] [#%d] Could not set option: %d (result: %d)", this, _socket.GetSocket(), option, result);
			return false;
		}

		return true;
	}

	bool Socket::SetSockOpt(SRT_SOCKOPT option, const void *value, int value_length)
	{
		CHECK_STATE(!= SocketState::Closed, false);

		int result = ::srt_setsockopt(_socket.GetSocket(), 0, option, value, value_length);

		if(result == SRT_ERROR)
		{
			logtw("[%p] [#%d] Could not set option: %d (result: %s)", this, _socket.GetSocket(), option, srt_getlasterror_str());
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
		return _socket.GetType();
	}

	ssize_t Socket::Send(const void *data, size_t length, bool &is_retry)
	{
		// TODO: 별도 send queue를 만들어야 함
		//OV_ASSERT2(_socket.IsValid());
        is_retry = false;

		logtd("[%p] [#%d] Trying to send data %zu bytes...", this, _socket.GetSocket(), length);
		logtp("[%p] [#%d] %s", this, _socket.GetSocket(), ov::Dump(data, length, 64).CStr());

		auto data_to_send = static_cast<const uint8_t *>(data);
		size_t remained = length;
		size_t total_sent = 0L;
        int retry_count = 0;

		switch(GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				while(remained > 0L)
				{
					int sock = _socket.GetSocket();
					ssize_t sent = ::send(sock, data_to_send, remained, MSG_NOSIGNAL | (_is_nonblock ? MSG_DONTWAIT : 0));

					if(sent == -1L)
					{
						if(errno == EAGAIN)
						{
							// Wait for the send buffer
							fd_set write_fds {};
							FD_ZERO(&write_fds);
							FD_SET(sock, &write_fds);

							timeval tv {};
							tv.tv_sec = 0;
                            tv.tv_usec = 200000;

							int select_result = select(sock + 1, nullptr, &write_fds, nullptr, &tv);

							if(select_result > 0)
							{
								// send buffer is available
							}
							else if(select_result == 0)
							{
								// timed out
                                retry_count++;
                                if(retry_count > 5)
                                {
                                    is_retry = true;
                                    break;
                                }
							}
							else
							{
								logtw("[%p] [#%d] An error occurred while select(): %d (%s)", this, sock, select_result, ov::Error::CreateErrorFromErrno()->ToString().CStr());
								break;
							}

							continue;
						}

						logtw("[%p] [#%d] Could not send data: %zd (%s)", this, sock, sent, ov::Error::CreateErrorFromErrno()->ToString().CStr());

						break;
					}

					OV_ASSERT2(remained >= sent);

					remained -= sent;
					total_sent += sent;
					data_to_send += sent;
				}

				break;

			case SocketType::Srt:
			{
				SRT_MSGCTRL msgctrl {};

				// 10b == start of frame
				//msgctrl.boundary = 2;

				while(remained > 0L)
				{
					// SRT limits packet size up to 1316
					auto to_send = std::min(1316UL, remained);

					if(remained == to_send)
					{
						// 01b == end of frame
						//msgctrl.boundary |= 1;
					}

					int sent = ::srt_sendmsg2(_socket.GetSocket(), reinterpret_cast<const char *>(data_to_send), to_send, &msgctrl);

					if(sent == -1L)
					{
						if(errno == EAGAIN)
						{
							continue;
						}

						logtw("[%p] [#%d] Could not send data: %zd (%s)", this, _socket.GetSocket(), sent, ov::Error::CreateErrorFromSrt()->ToString().CStr());

						break;
					}

					OV_ASSERT2(remained >= sent);

					remained -= sent;
					total_sent += sent;
					data_to_send += sent;

					msgctrl.boundary = 0;
				}

				break;
			}

			case SocketType::Unknown:
				break;
		}

		logtd("[%p] [#%d] %zu bytes sent", this, _socket.GetSocket(), total_sent);

		return total_sent;
	}

	ssize_t Socket::Send(const void *data, size_t length)
	{
		OV_ASSERT2(data != nullptr);

		bool is_retry = false;
		return Send(data, length, is_retry);
	}

    ssize_t Socket::Send(const std::shared_ptr<const Data> &data)
    {
        OV_ASSERT2(data != nullptr);

        bool is_retry = false;
        return Send(data->GetData(), data->GetLength(), is_retry);
    }

	ssize_t Socket::SendTo(const ov::SocketAddress &address, const void *data, size_t length)
	{
		//OV_ASSERT2(_socket.IsValid());
		OV_ASSERT2(address.AddressForIPv4()->sin_addr.s_addr != 0);

		logtd("[%p] [#%d] Trying to send data %zu bytes to %s...", this, _socket.GetSocket(), length, address.ToString().CStr());

		switch(GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
			{
				while(true)
				{
					int result = ::sendto(_socket.GetSocket(), data, length, MSG_NOSIGNAL | (_is_nonblock ? MSG_DONTWAIT : 0), address.Address(), address.AddressLength());

					if(result >= 0)
					{
#if USE_STATS_COUNTER
						stats_counter.IncreasePps();
#endif // USE_STATS_COUNTER
					}
					else
					{
						if(errno == EAGAIN)
						{
#if USE_STATS_COUNTER
							stats_counter.IncreaseRetry();
#endif // USE_STATS_COUNTER
							continue;
						}

#if USE_STATS_COUNTER
						stats_counter.IncreaseError();
#endif // USE_STATS_COUNTER
					}

					return result;
				}
			}

			case SocketType::Srt:
				// Does not support SendTo() for SRT
				OV_ASSERT2(false);
				break;

			case SocketType::Unknown:
				break;
		}

		return -1;
	}

	ssize_t Socket::SendTo(const ov::SocketAddress &address, const std::shared_ptr<const Data> &data)
	{
		OV_ASSERT2(data != nullptr);

		return SendTo(address, data->GetData(), data->GetLength());
	}

	std::shared_ptr<ov::Error> Socket::Recv(std::shared_ptr<Data> &data)
	{
		//OV_ASSERT2(_socket.IsValid());
		OV_ASSERT2(data != nullptr);
		OV_ASSERT2(data->GetCapacity() > 0);

		logtd("[%p] [#%d] Trying to read from the socket...", this, _socket.GetSocket());

		data->SetLength(data->GetCapacity());

		ssize_t read_bytes = -1;

		SRT_MSGCTRL msg_ctrl {};

		switch(GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
				read_bytes = ::recv(_socket.GetSocket(), data->GetWritableData(), (size_t)data->GetLength(), (_is_nonblock ? MSG_DONTWAIT : 0));
				break;

			case SocketType::Srt:
				read_bytes = ::srt_recvmsg2(_socket.GetSocket(),
				                            reinterpret_cast<char *>(data->GetWritableData()),
				                            static_cast<int>(data->GetLength()), &msg_ctrl);
				break;

			default:
				break;
		}

		if(read_bytes == 0L)
		{
			logtd("[%p] [#%d] Client is disconnected (errno: %d)", this, _socket.GetSocket(), errno);

			data->SetLength(0L);

			return Error::CreateErrorFromErrno();
		}
		else if(read_bytes < 0L)
		{
			logtd("[%p] [#%d] recv() returns: %zd", this, _socket.GetSocket(), read_bytes);

			switch(GetType())
			{
				case SocketType::Udp:
				case SocketType::Tcp:
				{
					auto error = Error::CreateErrorFromErrno();

					data->SetLength(0L);

					switch(error->GetCode())
					{
						case EAGAIN:
							// 클라이언트가 보낸 데이터를 끝까지 다 읽었음. 다음 데이터가 올 때까지 대기해야 함
							logtd("[%p] [#%d] There is no data to read", this, _socket.GetSocket());
							break;

						case ECONNRESET:
							// Connection reset by peer
							logtw("[%p] [#%d] Connection reset by peer", this, _socket.GetSocket());
							SetState(SocketState::Error);
							return error;

						default:
							logte("[%p] [#%d] An error occurred while read data: %s", this, _socket.GetSocket(), error->ToString().CStr());
							SetState(SocketState::Error);

							logte("\n%s", ov::StackTrace::GetStackTrace().CStr());
							return error;
					}

					break;
				}

				case SocketType::Srt:
				{
					auto error = Error::CreateErrorFromSrt();

					data->SetLength(0);

					switch(error->GetCode())
					{
						case SRT_EASYNCRCV:
							logtd("[%p] [#%d] There is no data to read", this, _socket.GetSocket());
							break;

						case SRT_ECONNLOST:
							logtw("[%p] [#%d] Connection lost", this, _socket.GetSocket());
							SetState(SocketState::Error);
							return error;

						default:
							logte("[%p] [#%d] An error occurred while read data from SRT socket: %s", this, _socket.GetSocket(), error->ToString().CStr());
							SetState(SocketState::Error);
							return error;
					}

					break;
				}

				case SocketType::Unknown:
					break;
			}
		}
		else
		{
			logtd("[%p] [#%d] %zd bytes read", this, _socket.GetSocket(), read_bytes);

			data->SetLength(static_cast<size_t>(read_bytes));
		}

		return nullptr;
	}

	std::shared_ptr<ov::Error> Socket::RecvFrom(std::shared_ptr<Data> &data, std::shared_ptr<ov::SocketAddress> *address)
	{
		OV_ASSERT2(_socket.IsValid());
		OV_ASSERT2(data != nullptr);
		OV_ASSERT2(data->GetCapacity() > 0);

		switch(GetType())
		{
			case SocketType::Udp:
			case SocketType::Tcp:
			{
				sockaddr_in remote = { 0 };
				socklen_t remote_length = sizeof(remote);

				logtd("[%p] [#%d] Trying to read from the socket...", this, _socket.GetSocket());
				data->SetLength(data->GetCapacity());

				ssize_t read_bytes = ::recvfrom(_socket.GetSocket(), data->GetWritableData(), (size_t)data->GetLength(), (_is_nonblock ? MSG_DONTWAIT : 0), (sockaddr *)&remote, &remote_length);

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
							logtw("[%p] [#%d] Connection reset by peer", this, _socket.GetSocket());
							SetState(SocketState::Error);
							return error;

						default:
							logte("[%p] [#%d] An error occurred while read data: %s", this, _socket.GetSocket(), error->ToString().CStr());
							SetState(SocketState::Error);
							return error;
					}
				}
				else
				{
					logtd("[%p] [#%d] %zd bytes read", this, _socket.GetSocket(), read_bytes);

					data->SetLength(read_bytes);
					*address = std::make_shared<ov::SocketAddress>(remote);
				}
				break;
			}

			case SocketType::Srt:
				// Does not support RecvFrom() for SRT
				OV_ASSERT2(false);
				break;

			case SocketType::Unknown:
				break;
		}

		return nullptr;
	}

	bool Socket::Close()
	{
		SocketWrapper socket = _socket;

		if(_socket.IsValid())
		{
			logtd("[%p] [#%d] Trying to close socket...", this, socket.GetSocket());

			CHECK_STATE(!= SocketState::Closed, false);

			// socket 관련
			switch(GetType())
			{
				case SocketType::Tcp:
					if(_socket.IsValid())
					{
						// FIN 송신
						::shutdown(_socket.GetSocket(), SHUT_WR);
					}

					// It is intended that there is no "break;" statement here

				case SocketType::Udp:
					if(_socket.IsValid())
					{
						::close(_socket.GetSocket());
					}
					break;

				case SocketType::Srt:
					if(_socket.IsValid())
					{
						::srt_close(_socket.GetSocket());
					}
					break;

				default:
					break;
			}

			_socket.SetValid(false);

			// epoll 관련
			OV_SAFE_FUNC(_epoll, InvalidSocket, ::close,);
			OV_SAFE_FUNC(_srt_epoll, SRT_INVALID_SOCK, ::srt_epoll_release,);
			OV_SAFE_FREE(_epoll_events);

			logtd("[%p] [#%d] SocketBase is closed successfully", this, socket.GetSocket());

			SetState(SocketState::Closed);

			return true;
		}

		logtd("[%p] [#%d] Socket is already closed", this, socket.GetSocket());

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

	const char *StringFromSocketType(SocketType type)
	{
		switch(type)
		{
			case SocketType::Udp:
				return "UDP";

			case SocketType::Tcp:
				return "TCP";

			case SocketType::Srt:
				return "SRT";

			case SocketType::Unknown:
			default:
				return "Unknown";
		}
	}

	String Socket::ToString(const char *class_name) const
	{
		if(_socket == InvalidSocket)
		{
			return String::FormatString("<%s: %p, state: %d>", class_name, this, _state);
		}
		else
		{
			return String::FormatString(
				"<%s: %p, #%d, state: %d, %s, %s>",
				class_name, this,
				_socket.GetSocket(), _state,
				StringFromSocketType(GetType()),
				_remote_address->ToString().CStr()
			);
		}
	}

	String Socket::ToString() const
	{
		return ToString("Socket");
	}
}
