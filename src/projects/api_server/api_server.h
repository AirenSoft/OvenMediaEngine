//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/host.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>
#include <config/config.h>
#include <modules/http/server/http_server_manager.h>

#include "controllers/root_controller.h"

namespace api
{
	class Server : public ov::Singleton<Server>
	{
	public:
		bool Start(const std::shared_ptr<const cfg::Server> &server_config);
		bool Stop();

	protected:
		MAY_THROWS(std::shared_ptr<ConfigError>)
		void LoadAPIStorageConfigs(const cfg::mgr::api::Storage &storage_config);
		bool PrepareAPIStoragePath(const cfg::mgr::api::Storage &storage_config);
		bool PrepareHttpServers(const ov::String &server_ip, const cfg::mgr::Managers &managers, const cfg::bind::mgr::API &api_bind_config);

		std::shared_ptr<http::svr::RequestInterceptor> CreateInterceptor();

		std::shared_ptr<http::svr::HttpServer> _http_server;
		std::shared_ptr<http::svr::HttpsServer> _https_server;

		std::shared_ptr<RootController> _root_controller;

		ov::String _access_token;
	};
}  // namespace api