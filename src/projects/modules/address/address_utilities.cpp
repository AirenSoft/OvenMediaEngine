//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "address_utilities.h"

#include <modules/ice/stun_client.h>
#include <ifaddrs.h>

namespace ov
{
	bool AddressUtilities::ResolveMappedAddress(const ov::String &stun_server_addr)
	{
		auto addr_items = stun_server_addr.Split(":");
		if(addr_items.size() != 2)
		{
			return false;
		}

		return ResolveMappedAddress(ov::SocketAddress(addr_items[0], std::atoi(addr_items[1])));
	}

	bool AddressUtilities::ResolveMappedAddress(const ov::SocketAddress &stun_server_addr)
	{
		ov::SocketAddress mapped_address;

		if(StunClient::GetMappedAddress(stun_server_addr, mapped_address) == true)
		{
			_mapped_address = std::make_shared<ov::SocketAddress>(mapped_address);
			return true;
		}

		return false;
	}

	std::shared_ptr<ov::SocketAddress> AddressUtilities::GetMappedAddress()
	{
		return _mapped_address;
	}

	std::vector<ov::String> AddressUtilities::GetIpList(bool include_mapped_address)
	{
		std::vector<ov::String> list;

		if(include_mapped_address == true && _mapped_address != nullptr)
		{
			list.emplace_back(_mapped_address->GetIpAddress());
		}

		struct ifaddrs *interfaces = nullptr;
		int result = ::getifaddrs(&interfaces);
		if(result == 0)
		{
			struct ifaddrs *temp_addr = interfaces;

			while(temp_addr != nullptr)
			{
				// temp_addr->ifa_addr may be nullptr when the interface doesn't have any address
				if((temp_addr->ifa_addr != nullptr) && (temp_addr->ifa_addr->sa_family == AF_INET))
				{
					auto addr = (reinterpret_cast<struct sockaddr_in *>(temp_addr->ifa_addr))->sin_addr;

					// 0x0100007F = 127.0.0.1
					if(addr.s_addr != 0x0100007F)
					{
						char buffer[INET6_ADDRSTRLEN];
						::inet_ntop(temp_addr->ifa_addr->sa_family, &addr, buffer, INET6_ADDRSTRLEN);

						list.emplace_back(buffer);
					}
				}

				temp_addr = temp_addr->ifa_next;
			}

			::freeifaddrs(interfaces);
		}

		return std::move(list);
	}
}