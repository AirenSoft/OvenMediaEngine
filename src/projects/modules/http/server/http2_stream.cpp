//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http2_stream.h"

#include "../http_private.h"
#include "http_connection.h"

namespace http
{
	namespace svr
	{
		HttpStream::HttpStream(const std::shared_ptr<HttpConnection> &connection, uint32_t stream_id)
			: HttpTransaction(connection), _stream_id(stream_id)
		{
			// Default Header for HTTP 2.0
			_response->SetHeader("Server", "OvenMediaEngine");
			_response->SetHeader("Content-Type", "text/html");
		}

		// Get Stream ID
		uint32_t HttpStream::GetStreamId() const
		{
			return _stream_id;
		}

		bool HttpStream::ProcessFrame(const std::shared_ptr<const Http2Frame> &frame)
		{
			return true;
		}

	}  // namespace svr
}  // namespace http
