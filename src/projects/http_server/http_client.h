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

// HttpClient: Contains HttpRequest & HttpResponse
// HttpRequest: Contains request informations (Request HTTP Header & Body)
// HttpResponse: Contains socket & response informations (Response HTTP Header & Body)

class HttpClient
{
public:
	friend class HttpServer;

	HttpClient(const std::shared_ptr<HttpServer> &server, std::shared_ptr<HttpRequest> &http_request, std::shared_ptr<HttpResponse> &http_response);
	virtual ~HttpClient() = default;

	std::shared_ptr<HttpRequest> GetRequest();
	std::shared_ptr<HttpResponse> GetResponse();

	std::shared_ptr<const HttpRequest> GetRequest() const;
	std::shared_ptr<const HttpResponse> GetResponse() const;

protected:
	std::shared_ptr<HttpServer> _server = nullptr;

	std::shared_ptr<HttpRequest> _request = nullptr;
	std::shared_ptr<HttpResponse> _response = nullptr;
};
