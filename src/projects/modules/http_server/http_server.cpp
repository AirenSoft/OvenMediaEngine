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

#include "http_private.h"
#include "http_server_manager.h"

#define HTTP_SERVER_DEFAULT_WORKER_COUNT 4
#define HTTP_SERVER_DEFAULT_SOCKET_WORKER_THREAD_COUNT 4

HttpServer::HttpServer(const char *server_name)
	: _server_name(server_name)
{
}

HttpServer::~HttpServer()
{
	// PhysicalPort should be stopped before release HttpServer
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
		client_list = std::move(_client_list);
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

ssize_t HttpServer::TryParseHeader(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data)
{
	auto request = client->GetRequest();

	OV_ASSERT2(request->ParseStatus() == HttpStatusCode::PartialContent);

	// 파싱이 필요한 상태 - ProcessData()를 호출하여 파싱 시도
	ssize_t processed_length = request->ProcessData(data);

	switch (request->ParseStatus())
	{
		case HttpStatusCode::OK:
			// 파싱이 이제 막 완료된 상태. 즉, 파싱이 완료된 후 최초 1번만 여기로 진입함
			break;

		case HttpStatusCode::PartialContent:
			// 데이터 더 필요 - 이 상태에서는 반드시 모든 데이터를 소진했어야 함
			OV_ASSERT2((processed_length >= 0LL) && (static_cast<size_t>(processed_length) == data->GetLength()));
			break;

		default:
			// 파싱 도중 오류 발생
			OV_ASSERT2(processed_length == -1L);
			break;
	}

	return processed_length;
}

std::shared_ptr<HttpClient> HttpServer::FindClient(const std::shared_ptr<ov::Socket> &remote)
{
	std::shared_lock<std::shared_mutex> guard(_client_list_mutex);

	auto item = _client_list.find(remote.get());

	if (item != _client_list.end())
	{
		return item->second;
	}

	return nullptr;
}

void HttpServer::ProcessData(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data)
{
	if (client != nullptr)
	{
		std::shared_ptr<HttpRequest> request = client->GetRequest();
		std::shared_ptr<HttpResponse> response = client->GetResponse();

		bool need_to_disconnect = false;

		switch (request->ParseStatus())
		{
			case HttpStatusCode::OK: {
				auto interceptor = request->GetRequestInterceptor();

				if (interceptor != nullptr)
				{
					// If the request is parsed, bypass to the interceptor
					need_to_disconnect = (interceptor->OnHttpData(client, data) == HttpInterceptorResult::Disconnect);
				}
				else
				{
					OV_ASSERT2(false);
					need_to_disconnect = true;
				}

				break;
			}

			case HttpStatusCode::PartialContent: {
				// Need to parse HTTP header
				ssize_t processed_length = TryParseHeader(client, data);

				if (processed_length >= 0)
				{
					if (request->ParseStatus() == HttpStatusCode::OK)
					{
						// Probe scheme
						if (IsWebSocketRequest(request) == true)
						{
							request->SetConnectionType(HttpRequestConnectionType::WebSocket);
						}
						else
						{
							request->SetConnectionType(HttpRequestConnectionType::HTTP);
						}

						// Parsing is completed
						bool found_interceptor = false;
						// Find interceptor for the request
						{
							std::shared_lock<std::shared_mutex> guard(_interceptor_list_mutex);

							for (auto &interceptor : _interceptor_list)
							{
								if (interceptor->IsInterceptorForRequest(client))
								{
									found_interceptor = true;
									request->SetRequestInterceptor(interceptor);
									break;
								}
							}
						}

						auto interceptor = request->GetRequestInterceptor();

						if (interceptor == nullptr)
						{
							response->SetStatusCode(HttpStatusCode::InternalServerError);
							need_to_disconnect = true;
							OV_ASSERT2(false);
						}

						auto remote = request->GetRemote();

						if (remote != nullptr)
						{
							logti("Client(%s) is requested uri: [%s]", remote->ToString().CStr(), request->GetUri().CStr());
						}

						if (found_interceptor == false)
						{
							logtw("No module could be found to handle this connection request : [%s]", request->GetUri().CStr());
						}

						need_to_disconnect = need_to_disconnect || (interceptor->OnHttpPrepare(client) == HttpInterceptorResult::Disconnect);
						need_to_disconnect = need_to_disconnect || (interceptor->OnHttpData(client, data->Subdata(processed_length)) == HttpInterceptorResult::Disconnect);
					}
					else if (request->ParseStatus() == HttpStatusCode::PartialContent)
					{
						// Need more data
					}
				}
				else
				{
					// An error occurred with the request
					request->GetRequestInterceptor()->OnHttpError(client, HttpStatusCode::BadRequest);
					need_to_disconnect = true;
				}

				break;
			}

			default:
				// 이전에 parse 할 때 오류가 발생했다면 response한 뒤 close() 했으므로, 정상적인 상황이라면 여기에 진입하면 안됨
				logte("Invalid parse status: %d", request->ParseStatus());
				OV_ASSERT2(false);
				need_to_disconnect = true;
				break;
		}

		if (need_to_disconnect)
		{
			// 연결을 종료해야 함
			response->Response();
			response->Close();
		}
	}
}

std::shared_ptr<HttpClient> HttpServer::ProcessConnect(const std::shared_ptr<ov::Socket> &remote)
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

	auto http_client = std::make_shared<HttpClient>(GetSharedPtr(), request, response);

	_client_list[remote.get()] = http_client;

	return std::move(http_client);
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
		ProcessData(client, data);
	}
	else
	{
		// Avoid processing data received from sockets that attempted to close when keep alive request
	}
}

void HttpServer::OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
{
	std::shared_ptr<HttpClient> client;

	{
		std::lock_guard<std::shared_mutex> guard(_client_list_mutex);

		auto client_iterator = _client_list.find(remote.get());

		if (client_iterator == _client_list.end())
		{
			logte("Could not find client %s from list", remote->ToString().CStr());
			OV_ASSERT2(false);
			return;
		}

		client = client_iterator->second;
		_client_list.erase(client_iterator);
	}

	auto request = client->GetRequest();
	auto response = client->GetResponse();

	if (reason == PhysicalPortDisconnectReason::Disconnect)
	{
		logti("The HTTP client(%s) has been disconnected from %s (%d)",
			  remote->GetRemoteAddress()->ToString().CStr(), _physical_port->GetAddress().ToString().CStr(), response->GetStatusCode());
	}
	else
	{
		logti("The HTTP client(%s) is disconnected from %s (%d)",
			  remote->GetRemoteAddress()->ToString().CStr(), _physical_port->GetAddress().ToString().CStr(), response->GetStatusCode());
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

bool HttpServer::AddInterceptor(const std::shared_ptr<HttpRequestInterceptor> &interceptor)
{
	std::lock_guard<std::shared_mutex> guard(_interceptor_list_mutex);

	// Find interceptor in the list
	auto item = std::find_if(_interceptor_list.begin(), _interceptor_list.end(), [&](std::shared_ptr<HttpRequestInterceptor> const &value) -> bool {
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

bool HttpServer::RemoveInterceptor(const std::shared_ptr<HttpRequestInterceptor> &interceptor)
{
	std::lock_guard<std::shared_mutex> guard(_interceptor_list_mutex);

	// Find interceptor in the list
	auto item = std::find_if(_interceptor_list.begin(), _interceptor_list.end(), [&](std::shared_ptr<HttpRequestInterceptor> const &value) -> bool {
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

	for (auto &client : _client_list)
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
	std::vector<std::shared_ptr<HttpClient>> temp_list;

	{
		std::shared_lock<std::shared_mutex> guard(_client_list_mutex);

		for (auto client_iterator : _client_list)
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

bool HttpServer::IsWebSocketRequest(const std::shared_ptr<const HttpRequest> &request)
{
	// RFC6455 - 4.2.1.  Reading the Client's Opening Handshake
	//
	// 1.   An HTTP/1.1 or higher GET request, including a "Request-URI"
	//      [RFC2616] that should be interpreted as a /resource name/
	//      defined in Section 3 (or an absolute HTTP/HTTPS URI containing
	//      the /resource name/).
	//
	// 2.   A |Host| header field containing the server's authority.
	//

	if ((request->GetMethod() == HttpMethod::Get) && (request->GetHttpVersionAsNumber() > 1.0))
	{
		if (
			// 3.   An |Upgrade| header field containing the value "websocket",
			//      treated as an ASCII case-insensitive value.
			(request->GetHeader("UPGRADE") == "websocket") &&

			// 4.   A |Connection| header field that includes the token "Upgrade",
			//      treated as an ASCII case-insensitive value.
			(request->GetHeader("CONNECTION").UpperCaseString().IndexOf("UPGRADE") >= 0L) &&

			// 5.   A |Sec-WebSocket-Key| header field with a base64-encoded (see
			//      Section 4 of [RFC4648]) value that, when decoded, is 16 bytes in
			//      length.
			request->IsHeaderExists("SEC-WEBSOCKET-KEY") &&

			// 6.   A |Sec-WebSocket-Version| header field, with a value of 13.
			(request->GetHeader("SEC-WEBSOCKET-VERSION") == "13"))
		{
			// 7.   Optionally, an |Origin| header field.  This header field is sent
			//      by all browser clients.  A connection attempt lacking this
			//      header field SHOULD NOT be interpreted as coming from a browser
			//      client.
			//
			// 8.   Optionally, a |Sec-WebSocket-Protocol| header field, with a list
			//      of values indicating which protocols the client would like to
			//      speak, ordered by preference.
			//
			// 9.   Optionally, a |Sec-WebSocket-Extensions| header field, with a
			//      list of values indicating which extensions the client would like
			//      to speak.  The interpretation of this header field is discussed
			//      in Section 9.1.
			//
			// 10.  Optionally, other header fields, such as those used to send
			//      cookies or request authentication to a server.  Unknown header
			//      fields are ignored, as per [RFC2616].

			logtd("%s is websocket request", request->ToString().CStr());

			return true;
		}
	}

	return false;
}
