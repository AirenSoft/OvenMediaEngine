//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_server_manager.h"

#include <orchestrator/orchestrator.h>

#include "http_private.h"

std::shared_ptr<HttpServer> HttpServerManager::CreateHttpServer(const ov::SocketAddress &address)
{
	std::shared_ptr<HttpServer> http_server = nullptr;

	{
		auto lock_guard = std::lock_guard(_http_servers_mutex);
		auto item = _http_servers.find(address);

		if (item != _http_servers.end())
		{
			http_server = item->second;

			// Assume that http_server is not HttpsServer
			auto https_server = std::dynamic_pointer_cast<HttpsServer>(http_server);

			if (https_server != nullptr)
			{
				logte("Cannot reuse instance: Requested HttpServer, but previous instance is HttpsServer (%s)", address.ToString().CStr());
				http_server = nullptr;
			}
		}
		else
		{
			// Create a new HTTP server
			http_server = std::make_shared<HttpServer>();

			if (http_server->Start(address))
			{
				_http_servers[address] = http_server;
			}
			else
			{
				// Failed to start
				http_server = nullptr;
			}
		}

		return http_server;
	}
}

std::shared_ptr<HttpsServer> HttpServerManager::CreateHttpsServer(const ov::SocketAddress &address, const std::shared_ptr<info::Certificate> &certificate)
{
	std::shared_ptr<HttpsServer> https_server = nullptr;

	{
		auto lock_guard = std::lock_guard(_http_servers_mutex);
		auto item = _http_servers.find(address);

		if (item != _http_servers.end())
		{
			auto http_server = item->second;

			// Assume that http_server is not HttpsServer
			https_server = std::dynamic_pointer_cast<HttpsServer>(http_server);

			if (https_server == nullptr)
			{
				logte("Cannot reuse instance: Requested HttpsServer, but previous instance is HttpServer (%s)", address.ToString().CStr());
			}
			else if (https_server->SetCertificate(certificate) == false)
			{
				logte("Could not set certificate: HttpsServer can use only one certificate at the same time");
				https_server = nullptr;
			}
			else
			{
				// HTTPS server is ready
			}
		}
		else
		{
			// Create a new HTTP server
			https_server = std::make_shared<HttpsServer>();

			if (https_server->SetCertificate(certificate))
			{
				if (https_server->Start(address))
				{
					_http_servers[address] = https_server;
				}
				else
				{
					// Failed to start HTTP server
					https_server = nullptr;
				}
			}
			else
			{
				logte("Could not set certificate: HttpsServer can use only one certificate at the same time");
				https_server = nullptr;
			}
		}

		return https_server;
	}
}

std::shared_ptr<HttpsServer> HttpServerManager::CreateHttpsServer(const ov::SocketAddress &address, const std::vector<std::shared_ptr<ocst::VirtualHost>> &virtual_host_list)
{
	// Check if TLS is enabled
	auto vhost_list = ocst::Orchestrator::GetInstance()->GetVirtualHostList();

	if (vhost_list.empty())
	{
		return nullptr;
	}

	// TODO(Dimiden): OME doesn't support SNI yet, so OME can handle only one certificate.
	const auto &host_info = vhost_list[0]->host_info;

	return CreateHttpsServer(address, host_info.GetCertificate());
}

bool HttpServerManager::ReleaseServer(const std::shared_ptr<HttpServer> &http_server)
{
	// TODO(dimiden): Need to implement release feature (by reference count)
	if (http_server != nullptr)
	{
		return http_server->Stop();
	}

	return false;
}
