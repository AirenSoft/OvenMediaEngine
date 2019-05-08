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
#include "http_private.h"

HttpClient::HttpClient(std::shared_ptr<ov::ClientSocket> socket, const std::shared_ptr<HttpRequestInterceptor> &interceptor)
{
	OV_ASSERT2(socket != nullptr);

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
    // response mutex(tls)
    std::unique_lock<std::mutex> lock(_response_guard);

    if(_response == nullptr)
    {
        return;
    }

	_response->SetTls(tls);
}

std::shared_ptr<ov::Tls> HttpClient::GetTls()
{
    // response mutex(tls)
    std::unique_lock<std::mutex> lock(_response_guard);

    if(_response == nullptr)
    {
        return nullptr;
    }

	return _response->GetTls();
}

void HttpClient::SetTlsData(const std::shared_ptr<const ov::Data> &data)
{
	OV_ASSERT2(_tls_read_data == nullptr);

    _tls_read_data = data;
}

ssize_t HttpClient::TlsRead(ov::Tls *tls, void *buffer, size_t length)
{
	if(_tls_read_data == nullptr)
	{
		return 0;
	}

	const void *data = _tls_read_data->GetData();
	size_t bytes_to_copy = std::min(length, _tls_read_data->GetLength());

	::memcpy(buffer, data, bytes_to_copy);

	if(_tls_read_data->GetLength() > bytes_to_copy)
	{
		// Data is remained
        _tls_read_data = _tls_read_data->Subdata(bytes_to_copy);
	}
	else
	{
        _tls_read_data = nullptr;
	}

	return bytes_to_copy;
}

ssize_t HttpClient::TlsWrite(ov::Tls *tls, const void *data, size_t length)
{
    // response mutex(tls)
    std::unique_lock<std::mutex> lock(_response_guard);

    if(_response == nullptr)
    {
        logte("HttpReponse is null");
        return 0;
    }

    if (_tls_write_to_response)
    {
        _response->AppendTlsData(data, length);
        return length;
    }

    return _response->_remote->Send(data, length);
}

void HttpClient::Send(const std::shared_ptr<ov::Data> &data)
{
	OV_ASSERT2(_response != nullptr);

	_response->AppendData(data);
}
