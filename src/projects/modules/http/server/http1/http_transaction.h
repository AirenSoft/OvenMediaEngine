//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../http_exchange.h"
#include "../../protocol/http2/http2_frame.h"
#include "http1_request.h"


namespace http
{
	namespace svr
	{
		namespace h1
		{
			// For HTTP/2.0
			class HttpTransaction : public HttpExchange
			{
			public:
				// Constructor
				HttpTransaction(const std::shared_ptr<HttpConnection> &connection);

				ssize_t OnRequestPacketReceived(const std::shared_ptr<const ov::Data> &data);

			private:
				std::shared_ptr<HttpRequest> CreateRequestInstance() override;
				// std::shared_ptr<HttpResponse> CreateResponseInstance() override;

				std::shared_ptr<Http1Request> _http1_request;

				size_t _received_header_size = 0;
				size_t _received_data_size = 0;
			};
		}  // namespace h1
	}  // namespace svr
}  // namespace http