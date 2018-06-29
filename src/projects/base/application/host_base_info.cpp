//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "host_base_info.h"

HostBaseInfo::HostBaseInfo()
{
}

HostBaseInfo::~HostBaseInfo()
{
}

const ov::String HostBaseInfo::GetIPAddress() const noexcept
{
    return _ip_address;
}

void HostBaseInfo::SetIPAddress(ov::String ip_address)
{
    _ip_address = ip_address;
}

const int HostBaseInfo::GetPort() const noexcept
{
    return _port;
}

void HostBaseInfo::SetPort(int port)
{
    _port = port;
}

const ov::String HostBaseInfo::GetProtocol() const noexcept
{
    return _protocol;
}

void HostBaseInfo::SetProtocol(ov::String protocol)
{
    _protocol = protocol;
}

const int HostBaseInfo::GetMaxConnection() const noexcept
{
    return _max_connection;
}

void HostBaseInfo::SetMaxConnection(int max_connection)
{
    _max_connection = max_connection;
}