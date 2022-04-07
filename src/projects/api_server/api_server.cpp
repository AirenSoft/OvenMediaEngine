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
	struct XmlWriter : pugi::xml_writer
	{
		ov::String result;

		void write(const void *data, size_t size) override
		{
			result.Append(static_cast<const char *>(data), size);
		}
	};

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
				_https_server = http_server_manager->CreateHttpsServer("APIServer", tls_address, certificate, false, worker_count);

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

	void Server::SetupCors(const cfg::mgr::api::API &api_config)
	{
		bool is_cors_parsed;
		auto cross_domains = api_config.GetCrossDomainList(&is_cors_parsed);

		if (is_cors_parsed)
		{
			// API server doesn't have VHost, so use dummy VHost
			auto vhost_app_name = info::VHostAppName::InvalidVHostAppName();
			_cors_manager.SetCrossDomains(vhost_app_name, cross_domains);
		}
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

	bool Server::Start(const std::shared_ptr<const cfg::Server> &server_config)
	{
		// API Server configurations
		const auto &managers_config = server_config->GetManagers();
		const auto &api_config = managers_config.GetApi();

		// Port configurations
		const auto &api_bind_config = server_config->GetBind().GetManagers().GetApi();

		if (api_bind_config.IsParsed() == false)
		{
			logti("API Server is disabled");
			return true;
		}

		SetupCors(api_config);
		if (SetupAccessToken(api_config) == false)
		{
			return false;
		}

		return PrepareHttpServers(server_config->GetIp(), managers_config, api_bind_config);
	}

	std::shared_ptr<http::svr::RequestInterceptor> Server::CreateInterceptor()
	{
		auto http_interceptor = std::make_shared<http::svr::DefaultInterceptor>();

		// CORS header processor
		http_interceptor->Register(http::Method::All, R"(.+)", [=](const std::shared_ptr<http::svr::HttpExchange> &client) -> http::svr::NextHandler {
			auto response = client->GetResponse();
			auto request = client->GetRequest();

			do
			{
				// Set default headers
				response->SetHeader("Server", "OvenMediaEngine");
				response->SetHeader("Content-Type", "text/html");

				auto vhost_app_name = info::VHostAppName::InvalidVHostAppName();

				_cors_manager.SetupHttpCorsHeader(vhost_app_name, request, response);

				return http::svr::NextHandler::Call;
			} while (false);

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

		std::shared_ptr<http::svr::HttpServer> http_server = std::move(_http_server);
		std::shared_ptr<http::svr::HttpsServer> https_server = std::move(_https_server);

		bool http_result = (http_server != nullptr) ? manager->ReleaseServer(http_server) : true;
		bool https_result = (https_server != nullptr) ? manager->ReleaseServer(https_server) : true;

		_is_storage_path_initialized = false;
		_storage_path = "";

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
}  // namespace api
