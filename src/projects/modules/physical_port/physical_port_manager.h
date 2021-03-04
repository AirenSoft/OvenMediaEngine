//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <memory>

#include "physical_port.h"
#include "physical_port_observer.h"

#define DEFAULT_PHYSICAL_PORT_WORKER_COUNT 4
#define DEFAULT_SOCKET_POOL_WORKER_COUNT 4

class PhysicalPortManager : public ov::Singleton<PhysicalPortManager>
{
public:
	friend class ov::Singleton<PhysicalPortManager>;

	virtual ~PhysicalPortManager();

	std::shared_ptr<PhysicalPort> CreatePort(ov::SocketType type,
											 const ov::SocketAddress &address,
											 int send_buffer_size = 0,
											 int recv_buffer_size = 0,
											 size_t worker_count = DEFAULT_PHYSICAL_PORT_WORKER_COUNT,
											 size_t socket_pool_worker_count = DEFAULT_SOCKET_POOL_WORKER_COUNT);

	bool DeletePort(std::shared_ptr<PhysicalPort> &port);

protected:
	PhysicalPortManager();

	std::shared_ptr<ov::SocketPool> GetSocketPool(const std::pair<ov::SocketType, ov::SocketAddress> &socket_info, int socket_pool_count);

	std::map<std::pair<ov::SocketType, ov::SocketAddress>, std::shared_ptr<PhysicalPort>> _port_list;
	std::map<std::pair<ov::SocketType, ov::SocketAddress>, std::shared_ptr<ov::SocketPool>> _socket_pool_list;

	std::mutex _port_list_mutex;
};
