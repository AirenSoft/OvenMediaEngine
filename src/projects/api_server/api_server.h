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
	class Storage
	{
	public:
		void Load(const cfg::mgr::api::Storage &storage_config);
		std::set<ov::String> Save();
		void Close();
		bool IsEnabled();

		void AddVhost(const ov::String &vhost);
		void DelVhost(const ov::String &vhost);

	private:
		void Reset();
		void CheckPath(const ov::String &path);
		void LoadXml();

		std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> GetVhosts();
		void SaveXml(const std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> &vhosts);

		bool _is_initialized { false };
		ov::String _file_path;
		std::set<ov::String> _vhost_names;
	};

	class Server : public ov::EnableSharedFromThis<Server>
	{
	public:
		bool Start(const std::shared_ptr<const cfg::Server> &server_config);
		bool Stop();

		MAY_THROWS(http::HttpError)
		void CreateVHost(const cfg::vhost::VirtualHost &vhost_config);
		MAY_THROWS(http::HttpError)
		void DeleteVHost(const info::Host &host_info);
		void StorageVHosts();

	protected:
		bool PrepareHttpServers(const ov::String &server_ip, const cfg::mgr::Managers &managers, const cfg::bind::mgr::API &api_bind_config);

		void SetupCors(const cfg::mgr::api::API &api_config);
		bool SetupAccessToken(const cfg::mgr::api::API &api_config);

		std::shared_ptr<http::svr::RequestInterceptor> CreateInterceptor();

		std::shared_ptr<http::svr::HttpServer> _http_server;
		std::shared_ptr<http::svr::HttpsServer> _https_server;

		std::shared_ptr<RootController> _root_controller;

		Storage _storage;
		ov::String _access_token;

		http::CorsManager _cors_manager;
	};
}  // namespace api
