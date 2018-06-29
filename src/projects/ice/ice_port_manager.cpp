//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "ice_port_manager.h"
#include "ice_private.h"

std::shared_ptr<IcePort> IcePortManager::CreatePort(ov::SocketType type, const ov::SocketAddress &address, std::shared_ptr<IcePortObserver> observer)
{
	auto key = std::make_pair(type, address);
	auto item = _mapping_table.find(key);
	std::shared_ptr<IcePort> ice_port = nullptr;

	if(item == _mapping_table.end())
	{
		// 새로 할당
		ice_port = std::make_shared<IcePort>();

		if(ice_port == nullptr)
		{
			// 할당 실패
			OV_ASSERT2(false);
		}
		else
		{
			logtd("Creating IcePort for %s (%s)...", address.ToString().CStr(), (type == ov::SocketType::Tcp) ? "TCP" : "UDP");

			if(ice_port->Create(type, address))
			{
				_mapping_table[key] = ice_port;

				logtd("IcePort is created successfully: %s", ice_port->ToString().CStr());
			}
			else
			{
				// 초기화 도중 오류 발생
				ice_port->Close();
				ice_port = nullptr;

				OV_ASSERT2(false);
			}
		}
	}
	else
	{
		// 기존에 할당된 인스턴스 사용
		ice_port = item->second;
	}

	if(ice_port != nullptr)
	{
		ice_port->AddObserver(observer);
	}

	return ice_port;
}

bool IcePortManager::ReleasePort(std::shared_ptr<IcePort> ice_port, std::shared_ptr<IcePortObserver> observer)
{
	auto key = std::make_pair(ice_port->GetType(), ice_port->GetAddress());
	auto item = _mapping_table.find(key);

	if(item == _mapping_table.end())
	{
		// 없는 포트를 해제하려고 시도함
		OV_ASSERT(false, "%s is not exists", ice_port->ToString().CStr());
		return false;
	}

	if(observer != nullptr)
	{
		if(ice_port->RemoveObserver(observer) == false)
		{
			OV_ASSERT(false, "An error occurred while remove observer: %p", observer.get());
			return false;
		}
	}
	else
	{
		if(ice_port->RemoveObservers() == false)
		{
			OV_ASSERT(false, "An error occurred while remove observers");
			return false;
		}
	}

	if(ice_port->HasObserver() == false)
	{
		// observer가 모두 삭제됨
		logtd("Deleting %s...", ice_port->ToString());

		_mapping_table.erase(item);
	}

	return true;
}