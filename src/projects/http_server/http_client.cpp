//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_request.h"
#include "http_response.h"
#include "http_client.h"

HttpClient::HttpClient(ov::ClientSocket *socket, const std::shared_ptr<HttpRequestInterceptor> &interceptor)
{
	OV_ASSERT2(socket);

	_request = std::make_shared<HttpRequest>(interceptor, socket);
	_response = std::make_shared<HttpResponse>(_request.get(), socket);

	OV_ASSERT2(_request != nullptr);
	OV_ASSERT2(_response != nullptr);

	if(_response != nullptr)
	{
		// Set default headers
		_response->SetHeader("Server", "OvenMediaEngine");
		_response->SetHeader("Content-Type", "text/html");
	}
}

void HttpClient::SetTls(const std::shared_ptr<ov::Tls> &tls)
{
	_response->SetTls(tls);
}

std::shared_ptr<ov::Tls> HttpClient::GetTls()
{
	return _response->GetTls();
}

std::shared_ptr<const ov::Tls> HttpClient::GetTls() const
{
	return _response->GetTls();
}

void HttpClient::SetTlsData(const std::shared_ptr<const ov::Data> &data)
{
	OV_ASSERT2(_tls_data == nullptr);

	_tls_data = data;
}

ssize_t HttpClient::TlsRead(ov::Tls *tls, void *buffer, size_t length)
{
	if(_tls_data == nullptr)
	{
		return 0;
	}

	const void *data = _tls_data->GetData();
	size_t bytes_to_copy = std::min(length, _tls_data->GetLength());

	::memcpy(buffer, data, bytes_to_copy);

	if(_tls_data->GetLength() > bytes_to_copy)
	{
		// Data is remained
		_tls_data = _tls_data->Subdata(bytes_to_copy);
	}
	else
	{
		_tls_data = nullptr;
	}

	return bytes_to_copy;
}

ssize_t HttpClient::TlsWrite(ov::Tls *tls, const void *data, size_t length)
{
	return _response->_remote->Send(data, length);
}

void HttpClient::Send(const std::shared_ptr<ov::Data> &data)
{
	OV_ASSERT2(_response != nullptr);

	_response->AppendData(data);
}
