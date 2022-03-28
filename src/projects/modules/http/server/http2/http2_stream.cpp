//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http2_stream.h"

#include "../http_connection.h"

namespace http
{
	namespace svr
	{
		namespace h2
		{
			HttpStream::HttpStream(const std::shared_ptr<HttpConnection> &connection, uint32_t stream_id)
				: HttpExchange(connection), _stream_id(stream_id)
			{
				// https://www.rfc-editor.org/rfc/rfc7540.html#section-5.1.1
				// A stream identifier of zero (0x0) is used for connection control messages
				if (_stream_id == 0)
				{
					SendInitialControlMessage();
				}
			}

			std::shared_ptr<HttpRequest> HttpStream::GetRequest() const
			{
				return nullptr;
			}
			std::shared_ptr<HttpResponse> HttpStream::GetResponse() const
			{
				return nullptr;
			}

			// Get Stream ID
			uint32_t HttpStream::GetStreamId() const
			{
				return _stream_id;
			}

			bool HttpStream::OnFrameReceived(const std::shared_ptr<const Http2Frame> &frame)
			{
				switch (frame->GetType())
				{
					case Http2Frame::Type::Data:
					case Http2Frame::Type::Headers:
					case Http2Frame::Type::Priority:
					case Http2Frame::Type::RstStream:
					case Http2Frame::Type::PushPromise:
					case Http2Frame::Type::Ping:
					case Http2Frame::Type::GoAway:
					case Http2Frame::Type::WindowUpdate:
					case Http2Frame::Type::Continuation:

					case Http2Frame::Type::Unknown:
					default:
						return false;
				}

				return true;
			}

			bool HttpStream::SendInitialControlMessage()
			{
				return true;
			}
		}  // namespace h2
	}	   // namespace svr
}  // namespace http
