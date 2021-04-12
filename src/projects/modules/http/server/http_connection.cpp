//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_connection.h"

#include "../http_private.h"
#include "http_server.h"

namespace http
{
	namespace svr
	{
		HttpConnection::HttpConnection(const std::shared_ptr<HttpServer> &server, std::shared_ptr<HttpRequest> &request, std::shared_ptr<HttpResponse> &response)
			: _server(std::move(server)),
			  _request(request),
			  _response(response)
		{
		}

		std::shared_ptr<HttpRequest> HttpConnection::GetRequest()
		{
			return _request;
		}

		std::shared_ptr<HttpResponse> HttpConnection::GetResponse()
		{
			return _response;
		}

		std::shared_ptr<const HttpRequest> HttpConnection::GetRequest() const
		{
			return _request;
		}

		std::shared_ptr<const HttpResponse> HttpConnection::GetResponse() const
		{
			return _response;
		}
	}  // namespace svr
}  // namespace http
