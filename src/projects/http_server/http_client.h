//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <mutex>
#include "http_request.h"
#include "http_response.h"

class HttpServer;

class HttpClient
{
public:
	friend class HttpServer;

	HttpClient(const std::shared_ptr<HttpServer> &server, const std::shared_ptr<ov::ClientSocket> &remote);
	virtual ~HttpClient() = default;

	std::shared_ptr<HttpRequest> &GetRequest();
	std::shared_ptr<HttpResponse> &GetResponse();
	std::shared_ptr<ov::ClientSocket> &GetRemote();

	std::shared_ptr<const HttpRequest> GetRequest() const;
	std::shared_ptr<const HttpResponse> GetResponse() const;
	std::shared_ptr<const ov::ClientSocket> GetRemote() const;

	bool IsConnected() const noexcept;

	// Send the data immediately
	// Can be used for response without content-length
	template <typename T>
	bool Send(const T *data)
	{
		return Send(data, sizeof(T));
	}
	virtual bool Send(const void *data, size_t length);
	virtual bool Send(const std::shared_ptr<const ov::Data> &data);

	bool SendChunkedData(const void *data, size_t length);
	bool SendChunkedData(const std::shared_ptr<const ov::Data> &data);

	virtual bool Close();

protected:
	bool Prepare(const std::shared_ptr<HttpClient> &http_client, const std::shared_ptr<ov::ClientSocket> &socket, std::shared_ptr<HttpRequestInterceptor> interceptor);
	
	bool Reset();

	std::shared_ptr<HttpServer> _server = nullptr;

	std::shared_ptr<ov::ClientSocket> _remote = nullptr;

	std::shared_ptr<HttpRequest> _request = nullptr;
	std::shared_ptr<HttpResponse> _response = nullptr;
};
