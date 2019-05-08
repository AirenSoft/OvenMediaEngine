//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "http_request.h"
#include "http_response.h"
#include <mutex>

class HttpClient
{
public:
	friend class HttpServer;
	friend class HttpsServer;

	HttpClient(std::shared_ptr<ov::ClientSocket> socket, const std::shared_ptr<HttpRequestInterceptor> &interceptor);
	virtual ~HttpClient() = default;

	std::shared_ptr<HttpRequest> &GetRequest()
	{
		return _request;
	}

	std::shared_ptr<HttpResponse> &GetResponse()
	{
		return _response;
	}

    void Release()
    {
        // response mutex(tls)
        std::unique_lock<std::mutex> lock(_response_guard);

	    if(_response != nullptr && _response->GetTls() != nullptr)
        {
            _response = nullptr;
	    }
    }


	void Send(const std::shared_ptr<ov::Data> &data);

	void MarkAsAccepted()
	{
		_is_tls_accepted = true;
	}

	bool IsAccepted() const
	{
		return _is_tls_accepted;
	}

    void SetTlsWriteToResponse(bool tls_write_to_response)
    {
        _tls_write_to_response = tls_write_to_response;
    }

protected:
	//--------------------------------------------------------------------
	// APIs which is related to TLS
	//--------------------------------------------------------------------
	void SetTls(const std::shared_ptr<ov::Tls> &tls);
	std::shared_ptr<ov::Tls> GetTls();

	void SetTlsData(const std::shared_ptr<const ov::Data> &data);

	//--------------------------------------------------------------------
	// Called by TLS module
	//--------------------------------------------------------------------
	ssize_t TlsRead(ov::Tls *tls, void *buffer, size_t length);
	ssize_t TlsWrite(ov::Tls *tls, const void *data, size_t length);

protected:
	std::shared_ptr<HttpRequest> _request = nullptr;
	std::shared_ptr<HttpResponse> _response = nullptr;
    std::mutex _response_guard;

	std::shared_ptr<const ov::Data> _tls_read_data = nullptr;
  	bool _is_tls_accepted = false;
  	bool _tls_write_to_response = false;
};
