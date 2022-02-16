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
#include <modules/http/cors/cors_manager.h>
#include <modules/http/server/http_server_manager.h>

#include "controllers/root_controller.h"

namespace api
{
	class Server : public ov::EnableSharedFromThis<Server>
	{
	public:
		bool Start(const std::shared_ptr<const cfg::Server> &server_config);
		bool Stop();

		MAY_THROWS(http::HttpError)
		void CreateVHost(const cfg::vhost::VirtualHost &vhost_config);
		MAY_THROWS(http::HttpError)
		void DeleteVHost(const info::Host &host_info);

	protected:
		bool PrepareHttpServers(const ov::String &server_ip, const cfg::mgr::Managers &managers, const cfg::bind::mgr::API &api_bind_config);

		void SetupCors(const cfg::mgr::api::API &api_config);
		bool SetupAccessToken(const cfg::mgr::api::API &api_config);

		std::shared_ptr<http::svr::RequestInterceptor> CreateInterceptor();

		std::shared_ptr<http::svr::HttpServer> _http_server;
		std::shared_ptr<http::svr::HttpsServer> _https_server;

		std::shared_ptr<RootController> _root_controller;

		ov::String _access_token;

		bool _is_storage_path_initialized = false;
		ov::String _storage_path;

		http::CorsManager _cors_manager;
	};
}  // namespace api
