//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./socket_utilities.h"
#include "./socket_address.h"

#include <ifaddrs.h>

namespace ov
{
	std::vector<ov::String> SocketUtilities::GetIpList()
	{
		std::vector<ov::String> list;

		struct ifaddrs *interfaces = nullptr;

		int result = ::getifaddrs(&interfaces);

		if(result == 0)
		{
			struct ifaddrs *temp_addr = interfaces;

			while(temp_addr != nullptr)
			{
				if(temp_addr->ifa_addr->sa_family == AF_INET)
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