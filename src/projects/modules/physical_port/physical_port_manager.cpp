//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "physical_port_manager.h"

#include "physical_port_private.h"

PhysicalPortManager::PhysicalPortManager()
{
}

PhysicalPortManager::~PhysicalPortManager()
{
}

std::shared_ptr<PhysicalPort> PhysicalPortManager::CreatePort(ov::SocketType type,
															  const ov::SocketAddress &address,
															  int send_buffer_size,
															  int recv_buffer_size,
															  size_t worker_count,
															  size_t socket_pool_worker_count)
{
	auto lock_guard = std::lock_guard(_port_list_mutex);

	auto key = std::make_pair(type, address);
	auto item = _port_list.find(key);
	std::shared_ptr<PhysicalPort> port = nullptr;

	if (item == _port_list.end())
	{
		port = std::make_shared<PhysicalPort>();

		if (port->Create(type, address, send_buffer_size, recv_buffer_size, worker_count, socket_pool_worker_count))
		{
			_port_list[key] = port;
		}
		else
		{
			port = nullptr;
		}
	}
	else
	{
		port = item->second;
	}

	if (port != nullptr)
	{
		port->IncreaseRefCount();

		if (port->GetWorkerCount() != worker_count)
		{
			logtw("The number of workers in the existing port differs from the number of workers passed by the argument: port: %zu, argument: %zu",
				port->GetWorkerCount(), worker_count);
		}

		if (port->GetSocketPoolWorkerCount() != socket_pool_worker_count)
		{
			logtw("The number of workers in the existing socket pool differs from the number of workers passed by the argument: socket pool: %zu, argument: %zu",
				port->GetSocketPoolWorkerCount(), socket_pool_worker_count);
		}
	}

	return port;
}

bool PhysicalPortManager::DeletePort(std::shared_ptr<PhysicalPort> &port)
{
	auto lock_guard = std::lock_guard(_port_list_mutex);

	auto key = std::make_pair(port->GetType(), port->GetAddress());
	auto item = _port_list.find(key);

	if (item == _port_list.end())
	{
		OV_ASSERT2(false);
		return false;
	}

	port->DecreaseRefCount();

	if (port->GetRefCount() == 0)
	{
		// last reference
		_port_list.erase(item);
		port->Close();
	}

	return true;
}
