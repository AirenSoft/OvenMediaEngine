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

HttpClient::HttpClient(const std::shared_ptr<HttpServer> &server, std::shared_ptr<HttpRequest> &request, std::shared_ptr<HttpResponse> &response)
	: _server(std::move(server)),
	  _request(request),
	  _response(response)
{
}

std::shared_ptr<HttpRequest> HttpClient::GetRequest()
{
	return _request;
}

std::shared_ptr<HttpResponse> HttpClient::GetResponse()
{
	return _response;
}

std::shared_ptr<const HttpRequest> HttpClient::GetRequest() const
{
	return _request;
}

std::shared_ptr<const HttpResponse> HttpClient::GetResponse() const
{
	return _response;
}
