//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"

class HostBaseInfo
{
public:
	HostBaseInfo();
	virtual ~HostBaseInfo();

	const ov::String GetIPAddress() const noexcept;
	void SetIPAddress(ov::String ip_address);

	const int GetPort() const noexcept;
	void SetPort(int port);

	const ov::String GetProtocol() const noexcept;
	void SetProtocol(ov::String protocol);

	const int GetMaxConnection() const noexcept;
	void SetMaxConnection(int max_connection);

protected:
	ov::String _ip_address;
	int _port;
	ov::String _protocol;
	int _max_connection;
};
