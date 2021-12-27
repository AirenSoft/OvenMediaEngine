//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_server.h"

#include <modules/physical_port/physical_port_manager.h>

#include "../http_private.h"
#include "http_server_manager.h"

namespace http
{
	namespace svr
	{
		HttpServer::HttpServer(const char *server_name)
			: _server_name(server_name)
		{
		}

		HttpServer::~HttpServer()
		{
			// PhysicalPort should be stopped before release Server
			OV_ASSERT(_physical_port == nullptr, "%s: Physical port: %s", _server_name.CStr(), _physical_port->ToString().CStr());
		}

		bool HttpServer::Start(const ov::SocketAddress &address, int worker_count)
		{
			auto lock_guard = std::lock_guard(_physical_port_mutex);

			if (_physical_port != nullptr)
			{
				logtw("Server is running");
				return false;
			}

			_physical_port = PhysicalPortManager::GetInstance()->CreatePort(_server_name.CStr(), ov::SocketType::Tcp, address, worker_count);

			if (_physical_port != nullptr)
			{
				return _physical_port->AddObserver(this);
			}

			return _physical_port != nullptr;
		}

		bool HttpServer::Stop()
		{
			std::shared_ptr<PhysicalPort> physical_port;

			{
				auto lock_guard = std::lock_guard(_physical_port_mutex);
				physical_port = std::move(_physical_port);
			}

			if (physical_port == nullptr)
			{
				// Server is not running
				return false;
			}

			physical_port->RemoveObserver(this);
			PhysicalPortManager::GetInstance()->DeletePort(physical_port);
			physical_port = nullptr;

			// Clean up the client list
			ClientList client_list;

			{
				auto lock_guard = std::lock_guard(_client_list_mutex);
				client_list = std::move(_connection_list);
			}

			for (auto &client : client_list)
			{
				client.second->GetResponse()->Close();
			}

			_interceptor_list.clear();

			return true;
		}

		bool HttpServer::IsRunning() const
		{
			auto lock_guard = std::lock_guard(_physical_port_mutex);

			return (_physical_port != nullptr);
		}

		std::shared_ptr<HttpConnection> HttpServer::FindClient(const std::shared_ptr<ov::Socket> &remote)
		{
			std::shared_lock<std::shared_mutex> guard(_client_list_mutex);

			auto item = _connection_list.find(remote.get());

			if (item != _connection_list.end())
			{
				return item->second;
			}

			return nullptr;
		}

		std::shared_ptr<HttpConnection> HttpServer::ProcessConnect(const std::shared_ptr<ov::Socket> &remote)
		{
			logti("Client(%s) is connected on %s", remote->ToString().CStr(), _physical_port->GetAddress().ToString().CStr());

			auto client_socket = std::dynamic_pointer_cast<ov::ClientSocket>(remote);

			if (client_socket == nullptr)
			{
				OV_ASSERT2(false);
				return nullptr;
			}

			auto request = std::make_shared<HttpRequest>(client_socket, _default_interceptor);
			auto response = std::make_shared<HttpResponse>(client_socket);

			if (response != nullptr)
			{
				// Set default headers
				response->SetHeader("Server", "OvenMediaEngine");
				response->SetHeader("Content-Type", "text/html");
			}

			std::lock_guard<std::shared_mutex> guard(_client_list_mutex);

			auto http_connection = std::make_shared<HttpConnection>(GetSharedPtr(), request, response);

			_connection_list[remote.get()] = http_connection;

			return http_connection;
		}

		void HttpServer::OnConnected(const std::shared_ptr<ov::Socket> &remote)
		{
			ProcessConnect(remote);
		}

		void HttpServer::OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
		{
			auto client = FindClient(remote);

			if (client == nullptr)
			{
				// This can be called in situations where the client closes the connection from the server at the same time as the data is sent
				return;
			}

			if (remote->IsClosing() == false)
			{
				client->ProcessData(data);
			}
			else
			{
				// Avoid processing data received from sockets that attempted to close when keep alive request
			}
		}

		void HttpServer::OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
		{
			std::shared_ptr<HttpConnection> client;

			{
				std::lock_guard<std::shared_mutex> guard(_client_list_mutex);

				auto client_iterator = _connection_list.find(remote.get());

				if (client_iterator == _connection_list.end())
				{
					// If an error occurs during TCP or HTTP connection processing, it may not exist in _connection_list.
					return;
				}

				client = client_iterator->second;
				_connection_list.erase(client_iterator);
			}

			auto request = client->GetRequest();
			auto response = client->GetResponse();

			if (reason == PhysicalPortDisconnectReason::Disconnect)
			{
				logti("Client(%s) has been disconnected from %s (%d)",
					  remote->ToString().CStr(), _physical_port->GetAddress().ToString().CStr(), response->GetStatusCode());
			}
			else
			{
				logti("Client(%s) is disconnected from %s (%d)",
					  remote->ToString().CStr(), _physical_port->GetAddress().ToString().CStr(), response->GetStatusCode());
			}

			auto interceptor = request->GetRequestInterceptor();

			if (interceptor != nullptr)
			{
				interceptor->OnHttpClosed(client, reason);
			}
			else
			{
				logtw("Interceptor does not exists for HTTP client %p", client.get());
			}
		}

		bool HttpServer::AddInterceptor(const std::shared_ptr<RequestInterceptor> &interceptor)
		{
			std::lock_guard<std::shared_mutex> guard(_interceptor_list_mutex);

			// Find interceptor in the list
			auto item = std::find_if(_interceptor_list.begin(), _interceptor_list.end(), [&](std::shared_ptr<RequestInterceptor> const &value) -> bool {
				return value == interceptor;
			});

			if (item != _interceptor_list.end())
			{
				// interceptor exists in the list
				logtw("%p is already registered", interceptor.get());
				return false;
			}

			_interceptor_list.push_back(interceptor);
			return true;
		}

		std::shared_ptr<RequestInterceptor> HttpServer::FindInterceptor(const std::shared_ptr<HttpConnection> &client)
		{
			// Find interceptor for the request
			std::shared_lock<std::shared_mutex> guard(_interceptor_list_mutex);

			for (auto &interceptor : _interceptor_list)
			{
				if (interceptor->IsInterceptorForRequest(client))
				{
					return interceptor;
				}
			}

			return nullptr;
		}

		bool HttpServer::RemoveInterceptor(const std::shared_ptr<RequestInterceptor> &interceptor)
		{
			std::lock_guard<std::shared_mutex> guard(_interceptor_list_mutex);

			// Find interceptor in the list
			auto item = std::find_if(_interceptor_list.begin(), _interceptor_list.end(), [&](std::shared_ptr<RequestInterceptor> const &value) -> bool {
				return value == interceptor;
			});

			if (item == _interceptor_list.end())
			{
				// interceptor does not exists in the list
				logtw("%p is not found.", interceptor.get());
				return false;
			}

			_interceptor_list.erase(item);
			return true;
		}

		ov::Socket *HttpServer::FindClient(ClientIterator iterator)
		{
			std::shared_lock<std::shared_mutex> guard(_client_list_mutex);

			for (auto &client : _connection_list)
			{
				if (iterator(client.second))
				{
					return client.first;
				}
			}

			return nullptr;
		}

		bool HttpServer::DisconnectIf(ClientIterator iterator)
		{
			std::vector<std::shared_ptr<HttpConnection>> temp_list;

			{
				std::shared_lock<std::shared_mutex> guard(_client_list_mutex);

				for (auto client_iterator : _connection_list)
				{
					auto &client = client_iterator.second;

					if (iterator(client))
					{
						temp_list.push_back(client);
					}
				};
			}

			for (auto client_iterator : temp_list)
			{
				client_iterator->GetResponse()->Close();
			}

			return true;
		}
	}  // namespace svr
}  // namespace http
