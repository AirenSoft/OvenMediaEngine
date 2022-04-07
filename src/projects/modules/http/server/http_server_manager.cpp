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

#include "../http_private.h"

namespace http
{
	namespace svr
	{
		std::shared_ptr<HttpServer> HttpServerManager::CreateHttpServer(const char *instance_name, const ov::SocketAddress &address, int worker_count)
		{
			std::shared_ptr<HttpServer> http_server = nullptr;

			auto module_config = cfg::ConfigManager::GetInstance()->GetServer()->GetModules();
			auto http2_enabled = module_config.GetHttp2().IsEnabled();

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
						logte("Cannot reuse instance: Requested Server, but previous instance is HttpsServer (%s)", address.ToString().CStr());
						http_server = nullptr;
					}
					else
					{
						if (worker_count != HTTP_SERVER_USE_DEFAULT_COUNT)
						{
							auto physical_port = http_server->GetPhysicalPort();

							if (physical_port != nullptr)
							{
								if (physical_port->GetWorkerCount() != worker_count)
								{
									logtw("The number of workers in the existing physical port differs from the number of workers passed by the argument: physical port: %zu, argument: %zu",
										  physical_port->GetWorkerCount(), worker_count);
									logtw("Because worker counts are different, the first initialized count is used: %d", physical_port->GetWorkerCount());
								}
							}
						}
					}
				}
				else
				{
					// Create a new HTTP server
					http_server = std::make_shared<HttpServer>(instance_name);

					if (http_server->Start(address, worker_count, http2_enabled))
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

		std::shared_ptr<HttpsServer> HttpServerManager::CreateHttpsServer(const char *instance_name, const ov::SocketAddress &address, bool disable_http2_force, int worker_count)
		{
			std::shared_ptr<HttpsServer> https_server = nullptr;
			auto module_config = cfg::ConfigManager::GetInstance()->GetServer()->GetModules();
			auto http2_enabled = module_config.GetHttp2().IsEnabled();

			if (disable_http2_force == true)
			{
				http2_enabled = false;
			}

			auto lock_guard = std::lock_guard(_http_servers_mutex);
			auto item = _http_servers.find(address);

			if (item != _http_servers.end())
			{
				auto http_server = item->second;

				// Assume that http_server is not HttpsServer
				https_server = std::dynamic_pointer_cast<HttpsServer>(http_server);
				if (https_server == nullptr)
				{
					logte("Cannot reuse instance: Requested HttpsServer, but previous instance is Server (%s)", address.ToString().CStr());
				}

				if ((https_server->IsHttp2Enabled() == true && http2_enabled == false))
				{
					logtw("Attempting to use HTTP/2 for ports with address %s enabled as HTTP/1.1 only.", address.ToString().CStr());
				}
				else if ((https_server->IsHttp2Enabled() == false && http2_enabled == true))
				{
					logtw("The %s address is trying to use HTTP/1.1 on a port that is HTTP/2 enabled.", address.ToString().CStr());
				}
			}
			else
			{
				// Create a new HTTP server
				https_server = std::make_shared<HttpsServer>(instance_name);

				if (https_server->Start(address, worker_count, http2_enabled))
				{
					_http_servers[address] = https_server;
				}
				else
				{
					// Failed to start HTTP server
					https_server = nullptr;
				}
			}

			return https_server;
		}

		bool HttpServerManager::AppendCertificate(const ov::SocketAddress &address, const std::shared_ptr<const info::Certificate> &certificate)
		{
			auto https_server = GetHttpsServer(address);
			if (https_server == nullptr)
			{
				logte("Could not find https server(%s) to append certificate", address.ToString(false).CStr());
				return false;
			}

			auto error = https_server->AppendCertificate(certificate);
			if (error != nullptr)
			{
				logte("Could not set certificate to https server(%s) : %s", address.ToString(false).CStr(), error->What());
				return false;
			}

			return true;
		}

		bool HttpServerManager::RemoveCertificate(const ov::SocketAddress &address, const std::shared_ptr<const info::Certificate> &certificate)
		{
			auto https_server = GetHttpsServer(address);
			if (https_server == nullptr)
			{
				logte("Could not find https server(%s) to append certificate", address.ToString(false).CStr());
				return false;
			}

			auto error = https_server->RemoveCertificate(certificate);
			if (error != nullptr)
			{
				logte("Could not set certificate to https server(%s) : %s", address.ToString(false).CStr(), error->What());
				return false;
			}

			return true;
		}

		std::shared_ptr<HttpsServer> HttpServerManager::CreateHttpsServer(const char *instance_name, const ov::SocketAddress &address, const std::shared_ptr<const info::Certificate> &certificate, bool disable_http2_force, int worker_count)
		{
			auto https_server = CreateHttpsServer(instance_name, address, disable_http2_force, worker_count);
			if (https_server != nullptr)
			{
				auto error = https_server->AppendCertificate(certificate);
				if (error != nullptr)
				{
					logte("Could not set certificate: %s", error->What());
					https_server = nullptr;
				}
				else
				{
					// HTTPS server is ready
				}
			}

			return https_server;
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

		std::shared_ptr<HttpsServer> HttpServerManager::GetHttpsServer(const ov::SocketAddress &address)
		{
			std::shared_ptr<HttpsServer> https_server = nullptr;

			auto lock_guard = std::lock_guard(_http_servers_mutex);
			auto item = _http_servers.find(address);

			if (item != _http_servers.end())
			{
				auto http_server = item->second;

				// Assume that http_server is not HttpsServer
				https_server = std::dynamic_pointer_cast<HttpsServer>(http_server);
				if (https_server == nullptr)
				{
					logte("Cannot reuse instance: Requested HttpsServer, but previous instance is Server (%s)", address.ToString().CStr());
				}
			}

			return https_server;
		}
	}  // namespace svr
}  // namespace http
