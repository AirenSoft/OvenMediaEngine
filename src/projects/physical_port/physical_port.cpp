//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "physical_port.h"

#include <algorithm>

#define OV_LOG_TAG "PhyPort"

PhysicalPort::PhysicalPort()
	: _type(ov::SocketType::Unknown),
	  _tcp_socket(nullptr),
	  _udp_socket(nullptr),

	  _need_to_stop(true)
{
}

PhysicalPort::~PhysicalPort()
{
	OV_ASSERT2(_observer_list.size() == 0);
}

bool PhysicalPort::Create(ov::SocketType type, const ov::SocketAddress &address)
{
	OV_ASSERT2((_tcp_socket == nullptr) && (_udp_socket == nullptr));

	logtd("Trying to start server...");

	switch(type)
	{
		case ov::SocketType::Tcp:
		{
			auto socket = std::make_shared<ov::ServerSocket>();

			if(socket->Prepare(address))
			{
				_type = type;
				_tcp_socket = socket;

				_need_to_stop = false;

				auto proc = [&, socket]() -> void
				{
					auto client_callback = [&](ov::ClientSocket *client, ov::SocketConnectionState state) -> bool
					{
						switch(state)
						{
							case ov::SocketConnectionState::Connected:
							{
								logtd("New client is connected: %s", client->ToString().CStr());

								// observer들에게 알림
								auto func = std::bind(&PhysicalPortObserver::OnConnected, std::placeholders::_1, static_cast<ov::Socket *>(client));
								std::for_each(_observer_list.begin(), _observer_list.end(), func);

								break;
							}

							case ov::SocketConnectionState::Disconnected:
							{
								logtd("Client is disconnected: %s", client->ToString().CStr());

								// observer들에게 알림
								auto func = std::bind(&PhysicalPortObserver::OnDisconnected, std::placeholders::_1, static_cast<ov::Socket *>(client), PhysicalPortDisconnectReason::Disconnected, nullptr);
								std::for_each(_observer_list.begin(), _observer_list.end(), func);

								break;
							}
						}

						if(client->GetType() == ov::SocketType ::Tcp)
						{
							// TCP는 명시적으로 close하기 전까지 계속 사용해야 하므로 false 반환
							return false;
						}

						return true;
					};

					auto data_callback = [&](ov::ClientSocket *client, const std::shared_ptr<const ov::Data> &data) -> bool
					{
						logtd("Received data %d bytes:\n%s", data->GetLength(), data->Dump().CStr());

						// observer들에게 알림
						auto func = std::bind(&PhysicalPortObserver::OnDataReceived, std::placeholders::_1, static_cast<ov::Socket *>(client), std::ref(*(client->GetRemoteAddress().get())), std::ref(data));
						std::for_each(_observer_list.begin(), _observer_list.end(), func);

						// TCP는 명시적으로 close하기 전까지 계속 사용해야 하므로 false 반환
						return false;
					};

					while((_need_to_stop == false) && (socket->DispatchEvent(client_callback, data_callback, 500)))
					{
					}

					socket->Close();

					logtd("Server is stopped");
				};

				// thread 시작
				_thread = std::thread(proc);

				_address = address;

				return true;
			}
			break;
		}

		case ov::SocketType::Udp:
		{
			auto socket = std::make_shared<ov::DatagramSocket>();

			if(socket->Prepare(address))
			{
				_type = type;
				_udp_socket = socket;

				_need_to_stop = false;

				auto proc = [&, socket]() -> void
				{
					auto data_callback = [&](ov::DatagramSocket *socket, const ov::SocketAddress &remote_address, const std::shared_ptr<const ov::Data> &data) -> bool
					{
						logtd("Received data %d bytes:\n%s", data->GetLength(), data->Dump().CStr());

						// observer들에게 알림
						auto func = std::bind(&PhysicalPortObserver::OnDataReceived, std::placeholders::_1, socket, remote_address, std::ref(data));
						std::for_each(_observer_list.begin(), _observer_list.end(), func);

						// UDP는 1회용 소켓으로 사용
						return true;
					};

					while((_need_to_stop == false) && (socket->DispatchEvent(data_callback, 500)))
					{
					}

					socket->Close();

					logtd("Server is stopped");
				};

				// thread 시작
				_thread = std::thread(proc);

				_address = address;

				return true;
			}

			break;
		}
	}

	return false;
}

bool PhysicalPort::Close()
{
	_need_to_stop = true;

	if(_thread.joinable())
	{
		_thread.join();
	}

	_thread = std::thread();

	switch(_type)
	{
		case ov::SocketType::Tcp:
			if(_tcp_socket != nullptr)
			{
				if(_tcp_socket->Close())
				{
					_tcp_socket = nullptr;
					_observer_list.clear();
					return true;
				}
			}
			break;

		case ov::SocketType::Udp:
			if(_udp_socket != nullptr)
			{
				_udp_socket = nullptr;
				_observer_list.clear();
				return true;
			}
			break;

		default:
			break;
	}

	return false;
}

ov::SocketState PhysicalPort::GetState()
{
	switch(_type)
	{
		case ov::SocketType::Tcp:
			OV_ASSERT2(_tcp_socket != nullptr);

			return _tcp_socket->GetState();

		case ov::SocketType::Udp:
			OV_ASSERT2(_udp_socket != nullptr);

			return _udp_socket->GetState();

		default:
			return ov::SocketState::Closed;
	}
}

bool PhysicalPort::AddObserver(PhysicalPortObserver *observer)
{
	_observer_list.push_back(observer);

	return true;
}

bool PhysicalPort::RemoveObserver(PhysicalPortObserver *observer)
{
	auto item = std::find(_observer_list.begin(), _observer_list.end(), observer);

	if(item == _observer_list.end())
	{
		return false;
	}

	_observer_list.erase(item);

	return true;
}
