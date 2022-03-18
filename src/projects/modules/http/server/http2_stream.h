//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "http_transaction.h"
#include "http2_frame.h"

namespace http
{
	namespace svr
	{
		// For HTTP/2.0
		class HttpStream : public HttpTransaction
		{
		public:
			friend class HttpConnection;

			HttpStream(const std::shared_ptr<HttpConnection> &connection, uint32_t stream_id);
			virtual ~HttpStream() = default;

			// Get Stream ID
			uint32_t GetStreamId() const;

		private:
			bool ProcessFrame(const std::shared_ptr<const Http2Frame> &frame);

			uint32_t _stream_id = 0;
		};
	}  // namespace svr
}  // namespace http