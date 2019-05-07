//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./socket_datastructure.h"

#include "../ovlibrary/string.h"

namespace ov
{
	/// IPv4/IPv6 주소 및 포트를 나타내는 클래스
	class SocketUtilities
	{
	public:
		// Get all IP addresses
		static std::vector<String> GetIpList();
	};
}