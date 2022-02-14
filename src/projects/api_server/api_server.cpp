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

	void Server::LoadAPIStorageConfigs(const cfg::mgr::api::Storage &storage_config)
	{
		auto storage_path = storage_config.GetPath();

		// Obtain a list of XML files in the storage path
		std::vector<ov::String> file_list;
		auto error = ov::PathManager::GetFileList(storage_path + "/", "VHost_*.xml", &file_list);

		if (error != nullptr)
		{
			if (error->GetCode() == ENOENT)
			{
				// The path could not be found - Ignore this error
				return;
			}

			// Another error occurred
			throw CreateConfigError("Failed to get a list of XML files in the storage path: %s (%s)", storage_path.CStr(), error->What());
		}

		// Load configurations from the API storage path
		logti("Trying to load API storage configurations in %s...", storage_path.CStr());

		if (file_list.empty() == false)
		{
			for (auto file_iterator = file_list.begin(); file_iterator != file_list.end(); ++file_iterator)
			{
				auto file_name = *file_iterator;

				cfg::DataSource data_source(cfg::DataType::Xml, storage_path, file_name, "VirtualHost");

				cfg::vhost::VirtualHost vhost_config;
				vhost_config.SetItemName("VirtualHost");
				vhost_config.FromDataSource(data_source);
				vhost_config.SetReadOnly(false);

				logti("Creating a new VirtualHost from %s...", file_name.CStr());
				CreateVHost(vhost_config, true);
			}
		}
		else
		{
			logti("There is no XML file in %s", storage_path.CStr());
		}
	}

	bool Server::PrepareAPIStoragePath(const cfg::mgr::api::Storage &storage_config)
	{
		_storage_path = "";
		_is_storage_path_initialized = false;

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

			_storage_path = storage_path;
		}
		else
		{
			logtw("API Storage is disabled. You will lose the configurations modified using the API.");
		}

		_is_storage_path_initialized = true;
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

		// CORS
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

		return PrepareHttpServers(server_config->GetIp(), managers, api_bind_config);
	}

	std::shared_ptr<http::svr::RequestInterceptor> Server::CreateInterceptor()
	{
		auto http_interceptor = std::make_shared<http::svr::DefaultInterceptor>();

		// CORS header processor
		http_interceptor->Register(http::Method::All, R"(.+)", [=](const std::shared_ptr<http::svr::HttpConnection> &client) -> http::svr::NextHandler {
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

	ov::String Server::MangleVHostName(const ov::String &vhost_name)
	{
		auto regex = ov::Regex::CompiledRegex("[^a-zA-Z0-9\\-_.\\[\\]\"']");

		return regex.Replace(vhost_name, "_", true);
	}

	ov::String Server::GenerateFileNameForVHostName(const ov::String &vhost_name)
	{
		ov::String file_name;

		file_name.Format("VHost_%s.xml", MangleVHostName(vhost_name).CStr());

		return ov::PathManager::Combine(_storage_path, file_name);
	}

	void Server::CreateVHost(const cfg::vhost::VirtualHost &vhost_config, bool write_to_storage)
	{
		if (_is_storage_path_initialized == false)
		{
			throw CreateConfigError("Storage path is not initialized (API server is not started)");
		}

		auto orchestrator = ocst::Orchestrator::GetInstance();

		OV_ASSERT2(vhost_config.IsReadOnly() == false);

		if (orchestrator->CreateVirtualHost(vhost_config) == ocst::Result::Failed)
		{
			throw CreateConfigError("Failed to create virtual host: %s", vhost_config.GetName().CStr());
		}

		if (write_to_storage)
		{
			ov::String file_name = GenerateFileNameForVHostName(vhost_config.GetName());

			logti("Writing [%s] config to %s...", vhost_config.GetName().CStr(), file_name.CStr());

			auto config = vhost_config.ToXml();
			auto comment_node = config.prepend_child(pugi::node_comment);
			ov::String comment;

			utsname uts{};
			::uname(&uts);

			comment.Format(
				"\n"
				"\tThis is an auto-generated by API.\n"
				"\tOvenMediaEngine may not work if this file is modified incorrectly.\n"
				"\t\n"
				"\tHost: %s (%s %s - %s, %s)\n"
				"\tOME Version: %s\n"
				"\tCreated at: %s\n",
				uts.nodename, uts.sysname, uts.machine, uts.release, uts.version,
				info::OmeVersion::GetInstance()->ToString().CStr(),
				ov::Time::MakeUtcMillisecond().CStr());

			comment_node.set_value(comment);

			auto declaration = config.prepend_child(pugi::node_declaration);
			declaration.append_attribute("version") = "1.0";
			declaration.append_attribute("encoding") = "utf-8";

			XmlWriter writer;
			config.print(writer);

			auto file = ov::DumpToFile(file_name, writer.result.CStr(), writer.result.GetLength());

			if (file == nullptr)
			{
				throw CreateConfigError("Could not write config to file: %s", file_name.CStr());
			}

			logti("[%s] is written to %s", vhost_config.GetName().CStr(), file_name.CStr());
		}
	}

	void Server::DeleteVHost(const info::Host &vhost_info, bool delete_from_storage)
	{
		if (vhost_info.IsReadOnly())
		{
			throw CreateConfigError("Could not delete virtual host: %s is not created by API call", vhost_info.GetName().CStr());
		}

		logti("Deleting virtual host: %s", vhost_info.GetName().CStr());

		auto result = ocst::Orchestrator::GetInstance()->DeleteVirtualHost(vhost_info);

		if (result != ocst::Result::Succeeded)
		{
			throw CreateConfigError("Failed to delete virtual host: %s (%d)", vhost_info.GetName().CStr(), ov::ToUnderlyingType(result));
		}

		if (delete_from_storage)
		{
			auto file_name = GenerateFileNameForVHostName(vhost_info.GetName());
			ov::String target_file_name = file_name;
			target_file_name.AppendFormat("_%ld.bak", ov::Time::GetTimestampInMs());

			auto error = ov::PathManager::Rename(file_name, target_file_name);

			if (error != nullptr)
			{
				throw CreateConfigError("Failed to delete config file for vhost: %s, error: (%d)", vhost_info.GetName().CStr(), error->What());
			}
		}
	}
}  // namespace api
