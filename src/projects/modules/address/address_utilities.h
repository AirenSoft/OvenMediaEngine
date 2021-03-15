//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/socket_address.h>

namespace ov
{
	class AddressUtilities : public ov::Singleton<AddressUtilities>
	{
	public:
		// Get mapped IP address from set stun server
		bool ResolveMappedAddress(const ov::SocketAddress &stun_server_addr);
		bool ResolveMappedAddress(const ov::String &stun_server_addr);

		// Get resolved mapped address
		std::shared_ptr<ov::SocketAddress> GetMappedAddress();

		// Get all IP addresses
		std::vector<String> GetIpList(bool include_mapped_address=true);

	private:
		std::shared_ptr<ov::SocketAddress>	_mapped_address = nullptr;
	};
}