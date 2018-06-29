//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "ice_port.h"
#include "ice_port_observer.h"

#include <functional>
#include <memory>
#include <map>

class IcePortManager : public ov::Singleton<IcePortManager>
{
public:
	friend class ov::Singleton<IcePortManager>;

	~IcePortManager() override = default;

	std::shared_ptr<IcePort> CreatePort(ov::SocketType type, const ov::SocketAddress &address, std::shared_ptr<IcePortObserver> observer);
	bool ReleasePort(std::shared_ptr<IcePort> ice_port, std::shared_ptr<IcePortObserver> observer);

protected:
	IcePortManager() = default;

	// key: [type, address]
	// value: IcePort
	std::map<std::pair<ov::SocketType, ov::SocketAddress>, std::shared_ptr<IcePort>> _mapping_table;
};
