//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/certificate.h>

#include "http_server.h"
#include "https_server.h"

class HttpServerManager : public ov::Singleton<HttpServerManager>
{
public:
	std::shared_ptr<HttpServer> CreateHttpServer(const ov::SocketAddress &address);
	std::shared_ptr<HttpsServer> CreateHttpsServer(const ov::SocketAddress &address, const std::shared_ptr<info::Certificate> &certificate);
	std::shared_ptr<HttpsServer> CreateHttpsServer(const ov::SocketAddress &address, const std::vector<std::shared_ptr<ocst::VirtualHost>> &virtual_host_list);

	bool ReleaseServer(const std::shared_ptr<HttpServer> &http_server);

protected:
	std::mutex _http_servers_mutex;
	std::map<ov::SocketAddress, std::shared_ptr<HttpServer>> _http_servers;
};
