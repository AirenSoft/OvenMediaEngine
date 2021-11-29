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

namespace http
{
	namespace svr
	{
		class HttpServer;
		class HttpsServer;

		// HttpConnection: Contains HttpRequest & HttpResponse
		// HttpRequest: Contains request informations (Request HTTP Header & Body)
		// HttpResponse: Contains socket & response informations (Response HTTP Header & Body)

		class HttpConnection : public ov::EnableSharedFromThis<HttpConnection>
		{
		public:
			friend class HttpServer;
			friend class HttpsServer;

			HttpConnection(const std::shared_ptr<HttpServer> &server, std::shared_ptr<HttpRequest> &http_request, std::shared_ptr<HttpResponse> &http_response);
			virtual ~HttpConnection() = default;

			std::shared_ptr<HttpRequest> GetRequest();
			std::shared_ptr<HttpResponse> GetResponse();

			std::shared_ptr<const HttpRequest> GetRequest() const;
			std::shared_ptr<const HttpResponse> GetResponse() const;

		protected:
			bool IsWebSocketRequest();

			// @return returns true if HTTP header parsed successfully, otherwise returns false
			ssize_t TryParseHeader(const std::shared_ptr<const ov::Data> &data);
			void ProcessData(const std::shared_ptr<const ov::Data> &data);

			std::shared_ptr<HttpServer> _server = nullptr;

			std::shared_ptr<HttpRequest> _request = nullptr;
			std::shared_ptr<HttpResponse> _response = nullptr;
		};
	}  // namespace svr
}  // namespace http
