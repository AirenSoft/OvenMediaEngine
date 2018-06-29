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
#include "http_request.h"
#include "http_response.h"

#include <base/ovlibrary/ovlibrary.h>

HttpMethod operator |(HttpMethod lhs, HttpMethod rhs)
{
	return static_cast<HttpMethod> (static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs));
}

HttpServer::HttpServer()
	: _default_interceptor(std::make_shared<HttpDefaultInterceptor>())
{
}

HttpServer::~HttpServer()
{
	OV_ASSERT2(_physical_port == nullptr);
}

bool HttpServer::Start(const ov::SocketAddress &address)
{
	if(_physical_port != nullptr)
	{
		logtw("Server is running");
		return false;
	}

	_physical_port = PhysicalPortManager::Instance()->CreatePort(ov::SocketType::Tcp, address);

	if(_physical_port != nullptr)
	{
		_physical_port->AddObserver(this);
	}

	return _physical_port != nullptr;
}

bool HttpServer::Stop()
{
	if(_physical_port == nullptr)
	{
		return false;
	}

	_physical_port->RemoveObserver(this);
	PhysicalPortManager::Instance()->DeletePort(_physical_port);
	_physical_port = nullptr;

	return true;
}

void HttpServer::OnConnected(ov::Socket *remote)
{
	logtd("Client is connected: %s", remote->ToString().CStr());

	auto response = std::make_shared<HttpResponse>(dynamic_cast<ov::ClientSocket *>(remote));

	// 기본 헤더 추가
	response->SetHeader("Server", "OvenMediaEngine");
	response->SetHeader("Content-Type", "text/html");

	_client_list[remote] = std::make_shared<HttpRequest>(response, _default_interceptor);
}

void HttpServer::OnDataReceived(ov::Socket *remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	auto item = _client_list.find(remote);

	bool need_to_disconnect = false;

	if(item != _client_list.end())
	{
		std::shared_ptr<HttpRequest> &request = item->second;
		std::shared_ptr<HttpResponse> &response = request->GetHttpResponse();

		if(request->ParseStatus() == HttpStatusCode::OK)
		{
			// 이미 파싱 된 상태라면, 바로 interceptor로 처리를 넘김
			request->GetRequestInterceptor()->OnHttpData(request, response, data);

			if(response->IsConnected() == false)
			{
				request->GetRequestInterceptor()->OnHttpClosed(request, response);
			}
		}
		else if(request->ParseStatus() == HttpStatusCode::PartialContent)
		{
			// 헤더 파싱이 필요한 상태
			ssize_t processed_length = TryParseHeader(address, data, request, response);

			if(processed_length >= 0)
			{
				if(request->ParseStatus() == HttpStatusCode::OK)
				{
					// 파싱이 완료됨

					// 해당 request를 처리할 interceptor를 찾음
					for(auto &interceptor : _interceptor_list)
					{
						if(interceptor->IsInterceptorForRequest(request, response))
						{
							request->SetRequestInterceptor(interceptor);
							break;
						}
					}

					request->GetRequestInterceptor()->OnHttpPrepare(request, response);

					if(response->IsConnected())
					{
						request->GetRequestInterceptor()->OnHttpData(request, response, data->Subdata(processed_length));
					}

					if(response->IsConnected() == false)
					{
						request->GetRequestInterceptor()->OnHttpClosed(request, response);
					}
				}
				else if(request->ParseStatus() == HttpStatusCode::PartialContent)
				{
					// 데이터가 더 필요함. 나머지 데이터가 더 들어오길 대기
				}
			}
			else
			{
				// 오류 발생
				request->GetRequestInterceptor()->OnHttpError(request, response, HttpStatusCode::BadRequest);
			}
		}
		else
		{
			// 이전에 parse 할 때 오류가 발생했다면 response한 뒤 close() 했으므로, 정상적인 상황이라면 여기에 진입하면 안됨
			logte("Invalid parse status: %d", request->ParseStatus());
			OV_ASSERT2(false);
		}

		if(response->IsConnected() == false)
		{
			// interceptor 안에서 연결이 해제되었음. client map에서 삭제
			logti("Connection is closed: %s", remote->ToString().CStr());
			_client_list.erase(item);
		}
	}
	else
	{
		// OnConnected()에서 추가했으므로, 반드시 client list에 있어야 함
		OV_ASSERT2(item != _client_list.end());
	}
}

void HttpServer::OnDisconnected(ov::Socket *remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
{
	logtd("Client is disconnected: %s", remote->ToString().CStr());

	_client_list.erase(remote);
}

ssize_t HttpServer::TryParseHeader(const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data, const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response)
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

bool HttpServer::AddInterceptor(const std::shared_ptr<HttpRequestInterceptor> &interceptor)
{
	// 기존에 등록된 processor가 있는지 확인
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
	// 기존에 등록된 processor가 있는지 확인
	auto item = std::find_if(_interceptor_list.begin(), _interceptor_list.end(), [&](std::shared_ptr<HttpRequestInterceptor> const &value) -> bool
	{
		return value == interceptor;
	});

	if(item == _interceptor_list.end())
	{
		// 기존에 등록되어 있음
		logtw("%p is not found.", interceptor.get());
		return false;
	}

	_interceptor_list.erase(item);
	return true;
}

std::shared_ptr<HttpDefaultInterceptor> HttpServer::GetDefaultInterceptor()
{
	std::shared_ptr<HttpDefaultInterceptor> default_interceptor = std::static_pointer_cast<HttpDefaultInterceptor>(_default_interceptor);

	return default_interceptor;
}

bool HttpServer::Disconnect(const ov::String &id)
{

}

