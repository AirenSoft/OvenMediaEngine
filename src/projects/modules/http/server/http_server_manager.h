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
			std::shared_ptr<HttpServer> CreateHttpServer(const char *instance_name, const ov::SocketAddress &address, int worker_count = HTTP_SERVER_USE_DEFAULT_COUNT);

			std::shared_ptr<HttpsServer> CreateHttpsServer(const char *instance_name, const ov::SocketAddress &address, bool disable_http2_force, int worker_count);
			bool AppendCertificate(const ov::SocketAddress &address, const std::shared_ptr<const info::Certificate> &certificate);
			bool RemoveCertificate(const ov::SocketAddress &address, const std::shared_ptr<const info::Certificate> &certificate);

			std::shared_ptr<HttpsServer> CreateHttpsServer(const char *instance_name, const ov::SocketAddress &address, const std::shared_ptr<const info::Certificate> &certificate, bool disable_http2_force, int worker_count);
			
			std::shared_ptr<HttpsServer> GetHttpsServer(const ov::SocketAddress &address);
			bool ReleaseServer(const std::shared_ptr<HttpServer> &http_server);

		protected:
			std::mutex _http_servers_mutex;
			std::map<ov::SocketAddress, std::shared_ptr<HttpServer>> _http_servers;
		};
	}  // namespace svr
}  // namespace http
