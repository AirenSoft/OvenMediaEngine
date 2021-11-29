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

namespace http
{
	namespace svr
	{
		class HttpServerManager : public ov::Singleton<HttpServerManager>
		{
		public:
			std::shared_ptr<HttpServer> CreateHttpServer(const char *server_name, const ov::SocketAddress &address, int worker_count = HTTP_SERVER_USE_DEFAULT_COUNT);

			std::shared_ptr<HttpsServer> CreateHttpsServer(const char *server_name, const ov::SocketAddress &address, const std::vector<std::shared_ptr<ocst::VirtualHost>> &vhost_list, int worker_count = HTTP_SERVER_USE_DEFAULT_COUNT);
			std::shared_ptr<HttpsServer> CreateHttpsServer(const char *server_name, const ov::SocketAddress &address, const std::shared_ptr<const info::Certificate> &certificate, int worker_count = HTTP_SERVER_USE_DEFAULT_COUNT);

			bool ReleaseServer(const std::shared_ptr<HttpServer> &http_server);

		protected:
			std::shared_ptr<HttpsServer> GetHttpsServer(const char *server_name, const ov::SocketAddress &address, int worker_count = HTTP_SERVER_USE_DEFAULT_COUNT);

		protected:
			std::mutex _http_servers_mutex;
			std::map<ov::SocketAddress, std::shared_ptr<HttpServer>> _http_servers;
		};
	}  // namespace svr
}  // namespace http
