//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_server.h"
#include "http_private.h"

#include <physical_port/physical_port_manager.h>

HttpServer::~HttpServer()
{
	// PhysicalPort should be stopped before release HttpServer
	OV_ASSERT2(_physical_port == nullptr);
}

bool HttpServer::Start(const ov::SocketAddress &address, int sned_buffer_size, int recv_buffer_size)
{
	if(_physical_port != nullptr)
	{
		logtw("Server is running");
		return false;
	}

	_physical_port = PhysicalPortManager::Instance()->CreatePort(ov::SocketType::Tcp,
                                                                address,
                                                                sned_buffer_size,
                                                                recv_buffer_size);

	if(_physical_port != nullptr)
	{
		return _physical_port->AddObserver(this);
	}

	return _physical_port != nullptr;
}

bool HttpServer::Stop()
{
	if(_physical_port == nullptr)
	{
		OV_ASSERT2(false);
		return false;
	}

	// client들 정리
	_client_list_mutex.lock();
	auto client_list = std::move(_client_list);
	_client_list_mutex.unlock();

	for(auto &client : client_list)
	{
		Disconnect(client.second);
	}

	_physical_port->RemoveObserver(this);
	PhysicalPortManager::Instance()->DeletePort(_physical_port);
	_physical_port = nullptr;

	return true;
}

ssize_t HttpServer::TryParseHeader(const std::shared_ptr<const ov::Data> &data, const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response)
{
	OV_ASSERT2(request->ParseStatus() == HttpStatusCode::PartialContent);

	// 파싱이 필요한 상태 - ProcessData()를 호출하여 파싱 시도
	ssize_t processed_length = request->ProcessData(data);

	switch(request->ParseStatus())
	{
		case HttpStatusCode::OK:
			// 파싱이 이제 막 완료된 상태. 즉, 파싱이 완료된 후 최초 1번만 여기로 진입함
			break;

		case HttpStatusCode::PartialContent:
			// 데이터 더 필요 - 이 상태에서는 반드시 모든 데이터를 소진했어야 함
			OV_ASSERT2(processed_length == data->GetLength());
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
	std::lock_guard<std::mutex> guard(_client_list_mutex);

	auto item = _client_list.find(remote.get());

	if(item != _client_list.end())
	{
		return item->second;
	}

	return nullptr;
}

void HttpServer::OnConnected(const std::shared_ptr<ov::Socket> &remote)
{
	logti("Client(%s) is connected on %s", remote->ToString().CStr(), _physical_port->GetAddress().ToString().CStr());

	std::lock_guard<std::mutex> guard(_client_list_mutex);

	_client_list[remote.get()] = std::make_shared<HttpClient>(std::dynamic_pointer_cast<ov::ClientSocket>(remote), _default_interceptor);
}

void HttpServer::OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	auto client = FindClient(remote);

	ProcessData(client, data);
}

void HttpServer::ProcessData(std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data)
{
	OV_ASSERT2(client != nullptr);

	if(client != nullptr)
	{
		std::shared_ptr<HttpRequest> &request = client->GetRequest();
		std::shared_ptr<HttpResponse> &response = client->GetResponse();

		bool need_to_disconnect = false;

		switch(request->ParseStatus())
		{
			case HttpStatusCode::OK:
			{
				auto &interceptor = request->GetRequestInterceptor();

				if(interceptor != nullptr)
				{
					// If the request is parsed, bypass to the interceptor
					need_to_disconnect = (interceptor->OnHttpData(request, response, data) == false);

					if(need_to_disconnect)
					{
						need_to_disconnect = need_to_disconnect;
					}
				}
				else
				{
					OV_ASSERT2(false);
					need_to_disconnect = true;
				}

				break;
			}

			case HttpStatusCode::PartialContent:
			{
				// Need to parse HTTP header
				ssize_t processed_length = TryParseHeader(data, request, response);

				if(processed_length >= 0)
				{
					if(request->ParseStatus() == HttpStatusCode::OK)
					{
						// Parsing is completed

						// Find interceptor for the request
						{
							std::lock_guard<std::mutex> guard(_interceptor_list_mutex);

							for(auto &interceptor : _interceptor_list)
							{
								if(interceptor->IsInterceptorForRequest(request, response))
								{
									request->SetRequestInterceptor(interceptor);
									break;
								}
							}
						}

						auto interceptor = request->GetRequestInterceptor();

						if(interceptor == nullptr)
						{
							need_to_disconnect = true;
							OV_ASSERT2(false);
						}

						need_to_disconnect = need_to_disconnect || (interceptor->OnHttpPrepare(request, response) == false);
						need_to_disconnect = need_to_disconnect || (interceptor->OnHttpData(request, response, data->Subdata(processed_length)) == false);
					}
					else if(request->ParseStatus() == HttpStatusCode::PartialContent)
					{
						// Need more data
					}
				}
				else
				{
					// An error occurred with the request
					request->GetRequestInterceptor()->OnHttpError(request, response, HttpStatusCode::BadRequest);
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

		if(need_to_disconnect)
		{
			// 연결을 종료해야 함
			Disconnect(client);
		}
	}
}

void HttpServer::OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
{
	logti("Client(%s) is disconnected from %s", remote->GetRemoteAddress()->ToString().CStr(), _physical_port->GetAddress().ToString().CStr());

	DisconnectInternal(remote);
}

bool HttpServer::AddInterceptor(const std::shared_ptr<HttpRequestInterceptor> &interceptor)
{
	// 기존에 등록된 processor가 있는지 확인
	std::lock_guard<std::mutex> guard(_interceptor_list_mutex);

	auto item = std::find_if(_interceptor_list.begin(), _interceptor_list.end(), [&](std::shared_ptr<HttpRequestInterceptor> const &value) -> bool
	{
		return value == interceptor;
	});

	if(item != _interceptor_list.end())
	{
		// 기존에 등록되어 있음
		logtw("%p is already observer", interceptor.get());
		return false;
	}

	_interceptor_list.push_back(interceptor);
	return true;
}

bool HttpServer::RemoveInterceptor(const std::shared_ptr<HttpRequestInterceptor> &interceptor)
{
	std::lock_guard<std::mutex> guard(_interceptor_list_mutex);

	// Find interceptor in the list
	auto item = std::find_if(_interceptor_list.begin(), _interceptor_list.end(), [&](std::shared_ptr<HttpRequestInterceptor> const &value) -> bool
	{
		return value == interceptor;
	});

	if(item == _interceptor_list.end())
	{
		// interceptor is not exists in the list
		logtw("%p is not found.", interceptor.get());
		return false;
	}


	_interceptor_list.erase(item);
	return true;
}

ov::Socket *HttpServer::FindClient(ClientIterator iterator)
{
	std::lock_guard<std::mutex> guard(_client_list_mutex);

	for(auto &client : _client_list)
	{
		if(iterator(client.second))
		{
			return client.first;
		}
	}

	return nullptr;
}

bool HttpServer::Disconnect(ClientIterator iterator)
{
	std::shared_ptr<HttpClient> client;

	{
		std::lock_guard<std::mutex> guard(_client_list_mutex);

		for(auto client_iterator = _client_list.begin(); client_iterator != _client_list.end(); ++client_iterator)
		{
			if(iterator(client_iterator->second))
			{
				client = client_iterator->second;
				_client_list.erase(client_iterator);
			}
		}
	}

	return DisconnectInternal(client);
}

bool HttpServer::Disconnect(std::shared_ptr<HttpClient> client)
{
	return DisconnectInternal(client->GetRequest()->GetRemote());
}

bool HttpServer::Disconnect(const std::shared_ptr<ov::Socket> &remote)
{
	return DisconnectInternal(remote);
}

bool HttpServer::DisconnectInternal(const std::shared_ptr<ov::Socket> &remote)
{
	std::shared_ptr<HttpClient> client;

	{
		std::lock_guard<std::mutex> guard(_client_list_mutex);

		auto client_iterator = _client_list.find(remote.get());

		if(client_iterator != _client_list.end())
		{
			client = client_iterator->second;
			_client_list.erase(client_iterator);
		}
		else
		{
			// 서버가 Stop() 되자마자 Client 접속을 끊으면 이곳으로 진입 할 수 있음
		}
	}

	return DisconnectInternal(client);
}

bool HttpServer::DisconnectInternal(std::shared_ptr<HttpClient> client)
{
	if(client == nullptr)
	{
		return false;
	}

	auto request = client->GetRequest();
	auto response = client->GetResponse();
	auto interceptor = request->GetRequestInterceptor();

	request->SetRequestInterceptor(nullptr);

	_physical_port->DisconnectClient(request->GetRemote().get());

	if(interceptor != nullptr)
	{
		interceptor->OnHttpClosed(request, response);
	}
	
	// HttpClient shared_ptr use count down
	// HttpResponse release -> Tls Release -> HttpClient(client) Release
	client->Release();

	return true;
}
