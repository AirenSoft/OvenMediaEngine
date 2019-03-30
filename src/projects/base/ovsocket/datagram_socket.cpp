//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "datagram_socket.h"
#include "client_socket.h"
#include "socket_private.h"

namespace ov
{
	bool DatagramSocket::Prepare(int port)
	{
		return Prepare(SocketAddress(port));
	}

	bool DatagramSocket::Prepare(const SocketAddress &address)
	{
		CHECK_STATE(== SocketState::Closed, false);

		if(
			(
				Create(SocketType::Udp) &&
				MakeNonBlocking() &&
				PrepareEpoll() &&
				AddToEpoll(this, static_cast<void *>(this)) &&
				SetSockOpt<int>(SO_REUSEADDR, 1) &&
				Bind(address)
			) == false)
		{
			// 중간 과정에서 오류가 발생하면 실패 반환
			Close();
			return false;
		}

		return true;
	}

	bool DatagramSocket::DispatchEvent(const DatagramCallback& data_callback, int timeout)
	{
		CHECK_STATE2(>= SocketState::Created, <= SocketState::Bound, false);
		OV_ASSERT2(data_callback != nullptr);

		if(_is_nonblock)
		{
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

				if(
					OV_CHECK_FLAG(event->events, EPOLLERR) ||
					OV_CHECK_FLAG(event->events, EPOLLHUP) ||
					(!OV_CHECK_FLAG(event->events, EPOLLIN))
					)
				{
					// 오류 발생
					logte("[#%d] Epoll error: %p, events: %s (%d)", GetSocket(), epoll_data, StringFromEpollEvent(event).CStr(), event->events);
					_last_epoll_event_count = 0;
					return false;
				}
				else if(epoll_data == static_cast<void *>(this))
				{
					logtd("Trying to read UDP packets...");

					while(true)
					{
						std::shared_ptr<Data> data = std::make_shared<ov::Data>(UdpBufferSize);

						// socket에서 이벤트 발생
						std::shared_ptr<SocketAddress> remote;
						std::shared_ptr<ov::Error> error = RecvFrom(data, &remote);

						if(data->GetLength() > 0L)
						{
							data_callback(this->GetSharedPtrAs<DatagramSocket>(), *(remote.get()), data);
						}

						if(error != nullptr)
						{
							logtw("[#%d] An error occurred: %s", GetSocket(), error->ToString().CStr());
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

					logtd("All UDP data are processed");
				}
				else
				{
					// epoll에 자기 자신만 등록해놓은 상태이므로, 절대로 여기로 진입할 수 없음
					OV_ASSERT2(false);
					_last_epoll_event_count = 0;
					return false;
				}

				_last_epoll_event_count--;
			}

			OV_ASSERT2(_last_epoll_event_count == 0);
		}
		else
		{

		}

		return true;
	}

	String DatagramSocket::ToString() const
	{
		return Socket::ToString("DatagramSocket");
	}
}