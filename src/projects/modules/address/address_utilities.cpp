//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "address_utilities.h"

#include <ifaddrs.h>
#include <modules/ice/stun_client.h>

namespace ov
{
	bool AddressUtilities::ResolveMappedAddress(const ov::String &stun_server_addr)
	{
		const auto address = ov::SocketAddress::CreateAndGetFirst(stun_server_addr);

		if (address.IsValid() == false)
		{
			return false;
		}

		return ResolveMappedAddress(address);
	}

	bool AddressUtilities::ResolveMappedAddress(const ov::SocketAddress &stun_server_addr)
	{
		ov::SocketAddress mapped_address;

		if (StunClient::GetMappedAddress(stun_server_addr, mapped_address) == true)
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

	std::vector<ov::String> AddressUtilities::GetIpList(ov::SocketFamily family, bool include_mapped_address)
	{
		std::vector<ov::String> list;

		if (include_mapped_address && (_mapped_address != nullptr))
		{
			if (_mapped_address->GetFamily() == family)
			{
				list.emplace_back(_mapped_address->GetIpAddress());
			}
		}

		struct ifaddrs *interfaces = nullptr;
		int result = ::getifaddrs(&interfaces);

		if (result == 0)
		{
			struct ifaddrs *temp_addr = interfaces;

			while (temp_addr != nullptr)
			{
				// temp_addr->ifa_addr may be nullptr when the interface doesn't have any address
				if (temp_addr->ifa_addr != nullptr)
				{
					const auto addr_family = temp_addr->ifa_addr->sa_family;

					if (addr_family == static_cast<int>(family))
					{
						switch (family)
						{
							case ov::SocketFamily::Inet: {
								auto addr = ToSockAddrIn4(temp_addr->ifa_addr)->sin_addr;

								// 0x0100007F = 127.0.0.1
								if (addr.s_addr != 0x0100007F)
								{
									char buffer[INET_ADDRSTRLEN];
									::inet_ntop(addr_family, &addr, buffer, INET_ADDRSTRLEN);

									list.emplace_back(buffer);
								}

								break;
							}

							case ov::SocketFamily::Inet6: {
								auto addr = ToSockAddrIn6(temp_addr->ifa_addr);
								auto addr6 = &(addr->sin6_addr);
								auto is_link_local_address = IN6_IS_ADDR_LINKLOCAL(addr6);

								if (::memcmp(addr6, &in6addr_loopback, sizeof(in6addr_loopback)) != 0)
								{
									// The address doesn't indicates loopback address
									char buffer[INET6_ADDRSTRLEN];
									::inet_ntop(addr_family, addr6, buffer, INET6_ADDRSTRLEN);

									if (is_link_local_address)
									{
										// Append scope id for link-local address
										ov::String address(buffer);
										address.AppendFormat("%%%d", addr->sin6_scope_id);
										list.emplace_back(address);
									}
									else
									{
										list.emplace_back(buffer);
									}
								}
								break;
							}

							default:
								break;
						}
					}
				}

				temp_addr = temp_addr->ifa_next;
			}

			::freeifaddrs(interfaces);
		}

		return list;
	}

	void AddressUtilities::InetPton(int address_family, const char *__restrict address, void *__restrict __buf)
	{
		const int result = ::inet_pton(address_family, address, __buf);

		switch (result)
		{
			case 1:
				// succeeded
				return;

			case 0:
				throw SocketAddressError("Invalid address: %s", address);

			case -1:
				throw SocketAddressError(ov::Error::CreateErrorFromErrno(), "Could not convert address: %s", address);

			default:
				throw SocketAddressError("Could not convert address: %s (%d)", address, result);
		}
	}
}  // namespace ov