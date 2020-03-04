//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_client.h"
#include "http_private.h"
#include "http_request.h"
#include "http_response.h"
#include "http_server.h"

HttpClient::HttpClient(const std::shared_ptr<HttpServer> &server, const std::shared_ptr<ov::ClientSocket> &remote)
	: _server(std::move(server)),
	  _remote(remote)
{
	OV_ASSERT2(_server != nullptr);
	OV_ASSERT2(_remote != nullptr);
}

bool HttpClient::Prepare(const std::shared_ptr<HttpClient> &http_client, const std::shared_ptr<ov::ClientSocket> &socket, std::shared_ptr<HttpRequestInterceptor> interceptor)
{
	OV_ASSERT2(socket != nullptr);

	_request = std::make_shared<HttpRequest>(http_client, std::move(interceptor));
	_response = std::make_shared<HttpResponse>(http_client);

	OV_ASSERT2(_request != nullptr);
	OV_ASSERT2(_response != nullptr);

	if (_response != nullptr)
	{
		// Set default headers
		_response->SetHeader("Server", "OvenMediaEngine");
		_response->SetHeader("Content-Type", "text/html");
	}

	return true;
}

std::shared_ptr<HttpRequest> &HttpClient::GetRequest()
{
	return _request;
}

std::shared_ptr<HttpResponse> &HttpClient::GetResponse()
{
	return _response;
}

std::shared_ptr<ov::ClientSocket> &HttpClient::GetRemote()
{
	return _remote;
}

std::shared_ptr<const HttpRequest> HttpClient::GetRequest() const
{
	return _request;
}

std::shared_ptr<const HttpResponse> HttpClient::GetResponse() const
{
	return _response;
}

std::shared_ptr<const ov::ClientSocket> HttpClient::GetRemote() const
{
	return _remote;
}

bool HttpClient::IsConnected() const noexcept
{
	OV_ASSERT2(_remote != nullptr);
	return _remote->GetState() == ov::SocketState::Connected;
}

bool HttpClient::Send(const void *data, size_t length)
{
	return Send(std::make_shared<ov::Data>(data, length));
}

bool HttpClient::Send(const std::shared_ptr<const ov::Data> &data)
{
	if (data == nullptr)
	{
		OV_ASSERT2(data != nullptr);
		return false;
	}

	return (_remote->Send(data) == static_cast<ssize_t>(data->GetLength()));
}

bool HttpClient::SendChunkedData(const void *data, size_t length)
{
	return SendChunkedData(std::make_shared<ov::Data>(data, length));
}

bool HttpClient::SendChunkedData(const std::shared_ptr<const ov::Data> &data)
{
	if ((data == nullptr) || data->IsEmpty())
	{
		// Send a empty chunk
		return Send("0\r\n\r\n", 5);
	}

	bool result =
		// Send the chunk header
		Send(ov::String::FormatString("%x\r\n", data->GetLength()).ToData(false)) &&
		// Send the chunk payload
		Send(data) &&
		// Send a last data of chunk
		Send("\r\n", 2);

	return result;
}

bool HttpClient::Close()
{
	OV_ASSERT2(_remote != nullptr);

	if(_remote == nullptr)
	{
		return false;
	}

	return _remote->Close();
}

bool HttpClient::Reset()
{
	_request = nullptr;
	_response = nullptr;

	return true;
}
