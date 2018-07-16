//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "host_provider_info.h"

HostProviderInfo::HostProviderInfo()
{
}

HostProviderInfo::~HostProviderInfo()
{
}

ov::String HostProviderInfo::ToString() const
{
	ov::String result = ov::String::FormatString("{\"ip_address\": \"%s\", \"port\": %d, \"protocol\": \"%s\", \"max_connection\": %d}",
	                                             _ip_address.CStr(), _port, _protocol.CStr(), _max_connection);

	return result;
}