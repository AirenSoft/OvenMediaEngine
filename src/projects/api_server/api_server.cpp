//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "api_server.h"

#include <orchestrator/orchestrator.h>

#include "api_private.h"
#include "controllers/root_controller.h"

#define API_VERSION "1"

namespace api
{
	void Server::LoadAPIStorageConfigs(const cfg::mgr::api::Storage &storage_config)
	{
		return;
		auto storage_path = storage_config.GetPath();

		// Obtain a list of XML files in the storage path
		std::vector<ov::String> file_list;
		auto error = ov::PathManager::GetFileList(storage_path + "/", "*.xml", &file_list);

		if (error != nullptr)
		{
			if (error->GetCode() == ENOENT)
			{
				// The path could not be found - Ignore this error
				return;
			}

			// Another error occurred
			throw CreateConfigError("Failed to get a list of XML files in the storage path: %s (%s)", storage_path.CStr(), error->ToString().CStr());
		}

		// Load configurations from the API storage path
		auto orchestrator = ocst::Orchestrator::GetInstance();

		logti("Trying to load API storage configurations in %s...", storage_path.CStr());

		if (file_list.empty() == false)
		{
			for (auto file_iterator = file_list.begin(); file_iterator != file_list.end(); ++file_iterator)
			{
				auto file_name = *file_iterator;

				cfg::DataSource data_source(cfg::DataType::Xml, storage_path, file_name, "VirtualHost");

				cfg::vhost::VirtualHost vhost_config;
				vhost_config.FromDataSource("VirtualHost", data_source);
				vhost_config.SetReadOnly(false);

				logtc("%s", vhost_config.ToString().CStr());

				if (orchestrator->CreateVirtualHost(vhost_config) == ocst::Result::Failed)
				{
					throw CreateConfigError("Failed to create virtual host from file: %s", data_source.GetFileName().CStr());
				}
			}
		}
		else
		{
			logti("There is no XML file in %s", storage_path.CStr());
		}
	}

	bool Server::PrepareAPIStoragePath(const cfg::mgr::api::Storage &storage_config)
	{
		if (storage_config.IsEnabled())
		{
			// Check the write permission for <Storage>
			const auto &storage_path = storage_config.GetPath();

			if (ov::PathManager::IsDirectory(storage_path))
			{
				int result = ::access(storage_path.CStr(), W_OK);

				if (result != 0)
				{
					throw CreateConfigError("Write permission denied. Unable to write: %s", storage_path.CStr());
				}

				// writable
			}
			else
			{
				logti("Trying to create API storage directory: %s", storage_path.CStr());

				if (ov::PathManager::MakeDirectory(storage_path) == false)
				{
					logte("Could not create directory: %s", storage_path.CStr());
					return false;
				}

				logti("Directory is created: %s", storage_path.CStr());
			}
		}
		else
		{
			logtw("API Storage is disabled. You will lose the configurations modified using the API.");
		}

		return true;
	}

	bool Server::PrepareHttpServers(const ov::String &server_ip, const cfg::mgr::Managers &managers, const cfg::bind::mgr::API &api_bind_config)
	{
		auto http_server_manager = http::svr::HttpServerManager::GetInstance();
		auto http_interceptor = CreateInterceptor();

		bool http_server_result = true;
		bool is_parsed;

		auto worker_count = api_bind_config.GetWorkerCount(&is_parsed);
		worker_count = is_parsed ? worker_count : HTTP_SERVER_USE_DEFAULT_COUNT;

		auto &port = api_bind_config.GetPort(&is_parsed);
		ov::SocketAddress address;
		if (is_parsed)
		{
			address = ov::SocketAddress(server_ip, port.GetPort());

			_http_server = http_server_manager->CreateHttpServer("APISvr", address, worker_count);

			if (_http_server != nullptr)
			{
				_http_server->AddInterceptor(http_interceptor);
			}
			else
			{
				logte("Could not initialize http::svr::Server");
				http_server_result = false;
			}
		}

		bool https_server_result = true;
		auto &tls_port = api_bind_config.GetTlsPort(&is_parsed);
		ov::SocketAddress tls_address;
		if (is_parsed)
		{
			auto host_name_list = std::vector<ov::String>();

			for (auto &name : managers.GetHost().GetNameList())
			{
				host_name_list.push_back(name);
			}

			tls_address = ov::SocketAddress(server_ip, tls_port.GetPort());
			auto certificate = info::Certificate::CreateCertificate("api_server", host_name_list, managers.GetHost().GetTls());

			if (certificate != nullptr)
			{
				_https_server = http_server_manager->CreateHttpsServer("APIServer", tls_address, certificate, worker_count);

				if (_https_server != nullptr)
				{
					_https_server->AddInterceptor(http_interceptor);
				}
				else
				{
					logte("Could not initialize http::svr::HttpsServer");
					https_server_result = false;
				}
			}
		}

		if (http_server_result && https_server_result)
		{
			ov::String port_description;

			if (_http_server != nullptr)
			{
				port_description.AppendFormat("%s/%s", address.ToString().CStr(), ov::StringFromSocketType(ov::SocketType::Tcp));
			}

			if (_https_server != nullptr)
			{
				if (port_description.IsEmpty() == false)
				{
					port_description.Append(", ");
				}

				port_description.AppendFormat("TLS: %s/%s", tls_address.ToString().CStr(), ov::StringFromSocketType(ov::SocketType::Tcp));
			}

			// Everything is OK
			logti("API Server is listening on %s...", port_description.CStr());

			return true;
		}

		// Rollback
		http_server_manager->ReleaseServer(_http_server);
		http_server_manager->ReleaseServer(_https_server);

		return false;
	}

	bool Server::Start(const std::shared_ptr<const cfg::Server> &server_config)
	{
		// API Server configurations
		const auto &managers = server_config->GetManagers();
		const auto &api_config = managers.GetApi();

		// Port configurations
		const auto &api_bind_config = server_config->GetBind().GetManagers().GetApi();

		if (api_bind_config.IsParsed() == false)
		{
			logti("API Server is disabled");
			return true;
		}

		_access_token = api_config.GetAccessToken();

		if (_access_token.IsEmpty())
		{
#if DEBUG
			logtw("An empty AccessToken setting was found. This is only allowed on Debug builds for ease of development, and the Release build does not allow empty AccessToken.");
#else	// DEBUG
			logte("Empty AccessToken is not allowed");
#endif	// DEBUG
		}

		const auto &storage_config = server_config->GetManagers().GetApi().GetStorage();

		if (storage_config.IsParsed())
		{
			try
			{
				LoadAPIStorageConfigs(storage_config);
			}
			catch (std::shared_ptr<cfg::ConfigError> &error)
			{
				logte("An error occurred while load API config: %s", error->ToString().CStr());
				return false;
			}

			if (PrepareAPIStoragePath(storage_config) == false)
			{
				return false;
			}
		}
		else
		{
			logtw("<Server><Managers><API><Storage> is not specified. You will lose the configurations modified using the API.");
		}

		return PrepareHttpServers(server_config->GetIp(), managers, api_bind_config);
	}

	std::shared_ptr<http::svr::RequestInterceptor> Server::CreateInterceptor()
	{
		auto http_interceptor = std::make_shared<http::svr::DefaultInterceptor>();

		// Request Handlers will be added to http_interceptor
		_root_controller = std::make_shared<RootController>(_access_token);
		_root_controller->SetInterceptor(http_interceptor);
		_root_controller->PrepareHandlers();

		return http_interceptor;
	}

	bool Server::Stop()
	{
		auto manager = http::svr::HttpServerManager::GetInstance();

		std::shared_ptr<http::svr::HttpServer> http_server = std::move(_http_server);
		std::shared_ptr<http::svr::HttpsServer> https_server = std::move(_https_server);

		bool http_result = (http_server != nullptr) ? manager->ReleaseServer(http_server) : true;
		bool https_result = (https_server != nullptr) ? manager->ReleaseServer(https_server) : true;

		return http_result && https_result;
	}

}  // namespace api
