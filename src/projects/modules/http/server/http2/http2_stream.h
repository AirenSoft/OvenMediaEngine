//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../protocol/http2/http2_frame.h"
#include "../http_exchange.h"

namespace http
{
	using namespace prot::h2;
	namespace svr
	{
		namespace h2
		{
			// For HTTP/2.0
			class HttpStream : public HttpExchange
			{
			public:
				friend class HttpConnection;

				HttpStream(const std::shared_ptr<HttpConnection> &connection, uint32_t stream_id);
				virtual ~HttpStream() = default;

				// Get Stream ID
				uint32_t GetStreamId() const;

				bool OnFrameReceived(const std::shared_ptr<const Http2Frame> &frame);

			private:
				// Send Settings frame and Window_Update frame
				bool SendInitialControlMessage();

				std::shared_ptr<HttpRequest> CreateRequestInstance() override;
				std::shared_ptr<HttpResponse> CreateResponseInstance() override;

				uint32_t _stream_id = 0;
			};
		}  // namespace h2
	} // namespace svr
}  // namespace http