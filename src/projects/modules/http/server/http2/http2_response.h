//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "../http_response.h"
#include "../../protocol/http2/frames/http2_frames.h"
#include "../../hpack/encoder.h"

#define MAX_HTTP2_HEADER_SIZE (16 * 1024)

namespace http
{
	namespace svr
	{
		namespace h2
		{
			class Http2Response : public HttpResponse
			{
			public:
				// Constructor
				Http2Response(uint32_t stream_id, const std::shared_ptr<ov::ClientSocket> &client_socket, const std::shared_ptr<hpack::Encoder> &hpack_encoder)
					: HttpResponse(client_socket)
				{
					_stream_id = stream_id;
					_hpack_encoder = hpack_encoder;
				}

				bool Send(const std::shared_ptr<prot::h2::Http2Frame> &frame)
				{
					return HttpResponse::Send(frame->ToData());
				}

			private:
				uint32_t SendHeader() override
				{
					std::shared_ptr<ov::Data> response = std::make_shared<ov::Data>();
					size_t sent_size = 0;

					SetHeader(":status", ov::Converter::ToString(static_cast<uint16_t>(GetStatusCode())));

					for (const auto &[name, values] : GetResponseHeaderList())
					{
						for (const auto &value : values)
						{
							// https://httpwg.org/http2-spec/draft-ietf-httpbis-http2bis.html#section-8.2
							// Field names MUST be converted to lowercase when constructing an HTTP/2 message.
							auto header_block = _hpack_encoder->Encode({name.LowerCaseString(), value}, hpack::Encoder::EncodingType::LiteralWithIndexing);
							response->Append(header_block);

							// Send fragmented header block if the header block size is larger than MAX_HTTP2_HEADER_SIZE
							if (response->GetLength() > MAX_HTTP2_HEADER_SIZE)
							{
								// Send the header block
								auto headers_frame = std::make_shared<prot::h2::Http2HeadersFrame>(_stream_id);
								headers_frame->SetHeaderBlockFragment(response);

								if (Send(headers_frame) == false)
								{
									return 0;
								}

								sent_size += response->GetLength();
								// Reset
								response = std::make_shared<ov::Data>();
							}
						}
					}

					auto headers_frame = std::make_shared<prot::h2::Http2HeadersFrame>(_stream_id);
					headers_frame->SetHeaderBlockFragment(response);

					// Set flags
					headers_frame->SetEndHeaders();
					if (GetResponseDataSize() == 0)
					{
						headers_frame->SetEndStream();
					}

					if (Send(headers_frame) == false)
					{
						return 0;
					}

					sent_size += response->GetLength();

					return sent_size;
				}

				uint32_t SendPayload() override
				{
					return 0;
				}

				uint32_t _stream_id = 0;
				std::shared_ptr<hpack::Encoder> _hpack_encoder;
			};
		}
	}
}