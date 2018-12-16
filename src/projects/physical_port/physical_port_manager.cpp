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

std::shared_ptr<PhysicalPort> PhysicalPortManager::CreatePort(ov::SocketType type, const ov::SocketAddress &address)
{
	auto key = std::make_pair(type, address);
	auto item = _port_list.find(key);
	std::shared_ptr<PhysicalPort> port = nullptr;

	if(item == _port_list.end())
	{
		port = std::make_shared<PhysicalPort>();

		if(port->Create(type, address))
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
	// TODO: reference counting을 해서, 여러 곳에서 delete 하더라도 문제 없이 동작하도록 해야 함
	auto key = std::make_pair(port->GetType(), port->GetAddress());
	auto item = _port_list.find(key);

	if(item == _port_list.end())
	{
		OV_ASSERT2(false);
		return false;
	}

	_port_list.erase(item);
	return false;
}
