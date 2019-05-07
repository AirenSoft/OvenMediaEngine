//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "physical_port_manager.h"

PhysicalPortManager::PhysicalPortManager()
{
}

PhysicalPortManager::~PhysicalPortManager()
{
}

std::shared_ptr<PhysicalPort> PhysicalPortManager::CreatePort(ov::SocketType type,
                                                              const ov::SocketAddress &address,
                                                              int sned_buffer_size,
                                                              int recv_buffer_size)
{
	auto key = std::make_pair(type, address);
	auto item = _port_list.find(key);
	std::shared_ptr<PhysicalPort> port = nullptr;

	if(item == _port_list.end())
	{
		port = std::make_shared<PhysicalPort>();

		if(port->Create(type, address, sned_buffer_size, recv_buffer_size))
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

	return port;
}

bool PhysicalPortManager::DeletePort(std::shared_ptr<PhysicalPort> &port)
{
	auto key = std::make_pair(port->GetType(), port->GetAddress());
	auto item = _port_list.find(key);

	if(item == _port_list.end())
	{
		OV_ASSERT2(false);
		return false;
	}

	if(port.use_count() == 2)
	{
		// last reference
		_port_list.erase(item);
		port->Close();
	}

	return true;
}
