//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "physical_port_observer.h"

#include <memory>
#include <functional>
#include <thread>

#include <base/ovsocket/ovsocket.h>

// PhysicalPort는 여러 곳에서 공유해서 사용할 수 있음
// PhysicalPortObserver를 iteration 하면서 callback 할 수 있는 구조 필요
class PhysicalPort
{
public:
	PhysicalPort();
	virtual ~PhysicalPort();

	bool Create(ov::SocketType type,
                const ov::SocketAddress &address,
                int send_buffer_size = 0,
                int recv_buffer_size = 0);

	bool Close();

	ov::SocketState GetState();

	ov::SocketType GetType() const
	{
		return _type;
	}

	const ov::SocketAddress &GetAddress() const
	{
		return _address;
	}

	bool AddObserver(PhysicalPortObserver *observer);

	bool RemoveObserver(PhysicalPortObserver *observer);

	bool DisconnectClient(ov::ClientSocket *client_socket);

protected:
	bool CreateServerSocket(ov::SocketType type,
	                        const ov::SocketAddress &address,
	                        int send_buffer_size,
                            int recv_buffer_size);

	bool CreateDatagramSocket(ov::SocketType type, const ov::SocketAddress &address);

	std::shared_ptr<PhysicalPort> _self;

	ov::SocketType _type;
	ov::SocketAddress _address;

	std::shared_ptr<ov::ServerSocket> _server_socket;
	std::shared_ptr<ov::DatagramSocket> _datagram_socket;

	volatile bool _need_to_stop;
	std::thread _thread;

	std::vector<PhysicalPortObserver *> _observer_list;
};
