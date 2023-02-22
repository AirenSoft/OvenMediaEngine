//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "api_server.h"

#include <base/info/ome_version.h>
#include <orchestrator/orchestrator.h>
#include <sys/utsname.h>

#include "api_private.h"
#include "controllers/root_controller.h"

#define API_VERSION "1"

namespace api
{
	void Storage::Load(const cfg::mgr::api::Storage &storage_config)
	{
		Reset();

		const ov::String message { "You will lose the configurations modified using the API." };
		if (storage_config.IsParsed() == false || storage_config.IsEnabled() == false)
		{
			logtw("<Server><Managers><API><Storage> is not specified or is disabled. %s", message.CStr());
			return;
		}

		CheckPath(storage_config.GetPath());
		if (_is_initialized == true)
		{
			LoadXml();
		}
		else
		{
			logte("An Error occured while check API storage file permissions. %s", message.CStr());
		}
	}

	std::set<ov::String> Storage::Save()
	{
		if (_is_initialized == true)
		{
			if (_vhost_names.size() > 0)
			{
				SaveXml(GetVhosts());
			}
			else
			{
				ov::PathManager::DeleteFile(_file_path);
			}
		}

		return _vhost_names;
	}

	void Storage::Close()
	{
		Save();
		Reset();
	}

	bool Storage::IsEnabled()
	{
		return _is_initialized;
	}

	void Storage::AddVhost(const ov::String &vhost)
	{
		if (_is_initialized && _vhost_names.find(vhost) == _vhost_names.end())
		{
			_vhost_names.insert(vhost);
		}
	}

	void Storage::DelVhost(const ov::String &vhost)
	{
		if (_is_initialized && _vhost_names.find(vhost) != _vhost_names.end())
		{
			_vhost_names.erase(vhost);
		}
	}

	void Storage::Reset()
	{
		_is_initialized = false;
		_file_path = ov::String();
		_vhost_names.clear();
	}

	void Storage::CheckPath(const ov::String &path)
	{
		auto file_path { path };
		if (file_path.IsEmpty())
		{
			auto config_path { cfg::ConfigManager::GetInstance()->GetConfigPath() };
			file_path = ov::PathManager::Combine(config_path, "Storage.xml");
		}

		logti("Check API storage file permission: %s", file_path.CStr());
		if (ov::PathManager::IsFile(file_path))
		{
			if (::access(file_path.CStr(), W_OK | R_OK ) != 0)
			{
				loge("Write/Read permission denied. Unable to write: %s", file_path.CStr());
				return;
			}
		}
		else
		{
			auto directory_path { ov::PathManager::ExtractPath(file_path) };
			if (::access(directory_path.CStr(), W_OK | R_OK ) != 0)
			{
				loge("Write/Read permission denied. Unable to write in: %s", directory_path.CStr());
				return;
			}
		}

		_is_initialized = true;
		_file_path = file_path;
	}

	void Storage::LoadXml()
	{
		if (ov::PathManager::IsFile(_file_path) == false)
		{
			return;
		}

		logti("Trying to load settings, API storage file: %s", _file_path.CStr());

		cfg::vhost::VirtualHosts vhosts_config;

		try
		{
			cfg::DataSource data_source(cfg::DataType::Xml, _file_path, "VirtualHosts");
			vhosts_config.SetItemName("VirtualHosts");
			vhosts_config.FromDataSource(data_source);
			vhosts_config.SetReadOnly(false);
		}
		catch (const cfg::ConfigError &error)
		{
			logte("An error occurred while load API storage file: %s", error.What());
			return;
		}

		for (auto vhost_config : vhosts_config.GetVirtualHostList())
		{
			vhost_config.SetReadOnly(false);

			auto vhost_name { vhost_config.GetName() };
			auto result { ocst::Orchestrator::GetInstance()->CreateVirtualHost(vhost_config) };
			if (result == ocst::Result::Succeeded)
			{
				AddVhost(vhost_config.GetName());
				logti("Created a new VirtualHost '%s' from API storage file", vhost_name.CStr());
			}
			else
			{
				logti("Could not create a new VirtualHost '%s' from API storage file", vhost_name.CStr());
			}
		}
	}

	std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> Storage::GetVhosts()
	{
		uint32_t index { 0 };
		std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> vhosts;

		for (const auto &vhost_name : _vhost_names)
		{
			auto vhost { GetVirtualHost(vhost_name) };
			if (vhost != nullptr)
			{
				vhosts.insert( { index++, vhost } );
			}
		}

		return vhosts;
	}

	void Storage::SaveXml(const std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> &vhosts)
	{
		logti("Trying to save settings, API storage file: %s", _file_path.CStr());

		pugi::xml_document document;

		auto declaration_node = document.append_child(pugi::node_declaration);
		declaration_node.append_attribute("version") = "1.0";
		declaration_node.append_attribute("encoding") = "utf-8";

		try
		{
			cfg::serdes::GetVirtualHostListFromMetrics(document, vhosts, true);
		}
		catch (const cfg::ConfigError &error)
		{
			logte("An error occurred while load API storage file: %s", error.What());
			return;
		}

		bool is_success = document.save_file(_file_path.CStr());
		if (is_success != true)
		{
			logte("An Error occured while saving API storage file");
		}
	}


	struct XmlWriter : pugi::xml_writer
	{
		ov::String result;

		void write(const void *data, size_t size) override
		{
			result.Append(static_cast<const char *>(data), size);
		}
	};

	bool Server::SetupCORS(const cfg::mgr::api::API &api_config)
	{
		bool is_cors_parsed;
		auto cross_domains = api_config.GetCrossDomainList(&is_cors_parsed);

		if (is_cors_parsed)
		{
			// API server doesn't have VHost, so use dummy VHost
			auto vhost_app_name = info::VHostAppName::InvalidVHostAppName();
			_cors_manager.SetCrossDomains(vhost_app_name, cross_domains);
		}

		return true;
	}

	bool Server::SetupAccessToken(const cfg::mgr::api::API &api_config)
	{
		_access_token = api_config.GetAccessToken();

		if (_access_token.IsEmpty())
		{
#if DEBUG
			logtw("An empty <AccessToken> setting was found. This is only allowed on Debug builds for ease of development, and the Release build does not allow empty <AccessToken>.");
#else	// DEBUG
			logte("Empty <AccessToken> is not allowed");
			return false;
#endif	// DEBUG
		}

		return true;
	}

	bool Server::PrepareHttpServers(
		const std::vector<ov::String> &ip_list,
		const bool is_port_configured, const uint16_t port,
		const bool is_tls_port_configured, const uint16_t tls_port,
		const cfg::mgr::Managers &managers_config,
		const int worker_count)
	{
		auto http_server_manager = http::svr::HttpServerManager::GetInstance();

		std::vector<std::shared_ptr<http::svr::HttpServer>> http_server_list;
		std::vector<std::shared_ptr<http::svr::HttpsServer>> https_server_list;

		auto http_interceptor = CreateInterceptor();
		std::shared_ptr<info::Certificate> certificate;

		if (is_tls_port_configured)
		{
			certificate = info::Certificate::CreateCertificate(
				"api_server",
				managers_config.GetHost().GetNameList(),
				managers_config.GetHost().GetTls());
		}

		if (http_server_manager->CreateServers(
				"API Server", "APISvr",
				&http_server_list, &https_server_list,
				ip_list,
				is_port_configured, port,
				is_tls_port_configured, tls_port,
				certificate,
				false,
				[&](const ov::SocketAddress &address, bool is_https, const std::shared_ptr<http::svr::HttpServer> &http_server) {
					http_server->AddInterceptor(http_interceptor);
				},
				worker_count))
		{
			_http_server_list = std::move(http_server_list);
			_https_server_list = std::move(https_server_list);

			return true;
		}

		http_server_manager->ReleaseServers(&http_server_list);
		http_server_manager->ReleaseServers(&https_server_list);

		return false;
	}

	bool Server::Start(const std::shared_ptr<const cfg::Server> &server_config)
	{
		// API Server configurations
		const auto &managers_config = server_config->GetManagers();
		const auto &api_config = managers_config.GetApi();

		// Port configurations
		const auto &api_bind_config = server_config->GetBind().GetManagers().GetApi();

		if (api_bind_config.IsParsed() == false)
		{
			logti("API Server is disabled by configuration");
			return true;
		}

		bool is_configured;
		auto worker_count = api_bind_config.GetWorkerCount(&is_configured);
		worker_count = is_configured ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

		bool is_port_configured;
		auto &port_config = api_bind_config.GetPort(&is_port_configured);

		bool is_tls_port_configured;
		auto &tls_port_config = api_bind_config.GetTlsPort(&is_tls_port_configured);

		if ((is_port_configured == false) && (is_tls_port_configured == false))
		{
			logtw("API Server is disabled - No port is configured");
			return true;
		}

		_storage.Load(api_config.GetStorage());

		return SetupCORS(api_config) &&
			   SetupAccessToken(api_config) &&
			   PrepareHttpServers(
				   server_config->GetIPList(),
				   is_port_configured, port_config.GetPort(),
				   is_tls_port_configured, tls_port_config.GetPort(),
				   managers_config,
				   worker_count);
	}

	std::shared_ptr<http::svr::RequestInterceptor> Server::CreateInterceptor()
	{
		auto http_interceptor = std::make_shared<http::svr::DefaultInterceptor>();

		// CORS header processor
		http_interceptor->Register(http::Method::All, R"(.+)", [=](const std::shared_ptr<http::svr::HttpExchange> &client) -> http::svr::NextHandler {
			auto response = client->GetResponse();
			auto request = client->GetRequest();

			// Set default headers
			response->SetHeader("Server", "OvenMediaEngine");
			response->SetHeader("Content-Type", "text/html");

			// API Server uses OPTIONS/GET/POST/PUT/PATCH/DELETE methods
			_cors_manager.SetupHttpCorsHeader(
				info::VHostAppName::InvalidVHostAppName(),
				request, response,
				{http::Method::Options, http::Method::Get, http::Method::Post, http::Method::Put, http::Method::Patch, http::Method::Delete});

			return http::svr::NextHandler::Call;
		});

		// Preflight request processor
		http_interceptor->Register(http::Method::Options, R"(.+)", [=](const std::shared_ptr<http::svr::HttpExchange> &client) -> http::svr::NextHandler {
			// Respond 204 No Content for preflight request
			client->GetResponse()->SetStatusCode(http::StatusCode::NoContent);

			// Do not call the next handler to prevent 404 Not Found
			return http::svr::NextHandler::DoNotCall;
		});

		// Request Handlers will be added to http_interceptor
		_root_controller = std::make_shared<RootController>(_access_token);
		_root_controller->SetServer(GetSharedPtr());
		_root_controller->SetInterceptor(http_interceptor);
		_root_controller->PrepareHandlers();

		return http_interceptor;
	}

	bool Server::Stop()
	{
		auto manager = http::svr::HttpServerManager::GetInstance();

		_http_server_list_mutex.lock();
		auto http_server_list = std::move(_http_server_list);
		auto https_server_list = std::move(_https_server_list);
		_http_server_list_mutex.unlock();

		auto http_result = manager->ReleaseServers(&http_server_list);
		auto https_result = manager->ReleaseServers(&https_server_list);

		_storage.Close();
		_root_controller = nullptr;

		return http_result && https_result;
	}

	void Server::CreateVHost(const cfg::vhost::VirtualHost &vhost_config)
	{
		OV_ASSERT2(vhost_config.IsReadOnly() == false);

		switch (ocst::Orchestrator::GetInstance()->CreateVirtualHost(vhost_config))
		{
			case ocst::Result::Failed:
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Failed to create the virtual host: [%s]",
									  vhost_config.GetName().CStr());

			case ocst::Result::Succeeded:
				_storage.AddVhost(vhost_config.GetName());
				break;

			case ocst::Result::Exists:
				throw http::HttpError(http::StatusCode::Conflict,
									  "The virtual host already exists: [%s]",
									  vhost_config.GetName().CStr());

			case ocst::Result::NotExists:
				// CreateVirtualHost() never returns NotExists
				OV_ASSERT2(false);
				throw http::HttpError(http::StatusCode::InternalServerError,
									  "Unknown error occurred: [%s]",
									  vhost_config.GetName().CStr());
		}
	}

	void Server::DeleteVHost(const info::Host &host_info)
	{
		OV_ASSERT2(host_info.IsReadOnly() == false);

		logti("Deleting virtual host: %s", host_info.GetName().CStr());

		switch (ocst::Orchestrator::GetInstance()->DeleteVirtualHost(host_info))
		{
			case ocst::Result::Failed:
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Failed to delete the virtual host: [%s]",
									  host_info.GetName().CStr());

			case ocst::Result::Succeeded:
				_storage.DelVhost(host_info.GetName());
				break;

			case ocst::Result::Exists:
				// CreateVirtDeleteVirtualHostualHost() never returns Exists
				OV_ASSERT2(false);
				throw http::HttpError(http::StatusCode::InternalServerError,
									  "Unknown error occurred: [%s]",
									  host_info.GetName().CStr());

			case ocst::Result::NotExists:
				// CreateVirtualHost() never returns NotExists
				throw http::HttpError(http::StatusCode::NotFound,
									  "The virtual host not exists: [%s]",
									  host_info.GetName().CStr());
		}
	}

	void Server::StorageVHosts()
	{
		logti("Storing dynamic virtual hosts ...");

		if (_storage.IsEnabled() == false)
		{
			throw http::HttpError(http::StatusCode::MethodNotAllowed, "<Server><Managers><API><Storage> is not specified or is disabled.");
		}

		_storage.Save();
	}

}  // namespace api
