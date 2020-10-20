//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/info/host.h"
#include "orchestrator/orchestrator.h"
#include "http_server.h"

class HttpsServer : public HttpServer
{
public:
	void SetVirtualHostList(std::vector<std::shared_ptr<ocst::VirtualHost>>& vhost_list);

protected:
	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;
	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;

protected:
	std::vector<std::shared_ptr<ocst::VirtualHost>> 	_virtual_host_list;
};
