//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http2_response.h"
#include "../../http_private.h"

namespace http
{
	namespace svr
	{
		namespace h2
		{
			// Constructor
			Http2Response::Http2Response(uint32_t stream_id, const std::shared_ptr<ov::ClientSocket> &client_socket, const std::shared_ptr<hpack::Encoder> &hpack_encoder)
				: HttpResponse(client_socket)
			{
				_stream_id = stream_id;
				_hpack_encoder = hpack_encoder;
			}

			bool Http2Response::Send(const std::shared_ptr<prot::h2::Http2Frame> &frame)
			{
				return HttpResponse::Send(frame->ToData());
			}

			void Http2Response::SetKeepStream(bool keep_stream)
			{
				_keep_stream = keep_stream;
			}
			
			bool Http2Response::Send(const std::shared_ptr<prot::h2::Http2DataFrame> &data_frame, bool end_stream)
			{
				auto frame = std::make_shared<prot::h2::Http2DataFrame>(_stream_id);
				frame->SetData(data_frame->GetData());
				if (end_stream == true)
				{
					frame->SetEndStream();
				}

				return Send(frame);
			}

			uint32_t Http2Response::SendHeader()
			{
				std::shared_ptr<ov::Data> response = std::make_shared<ov::Data>();
				size_t sent_size = 0;

				// :status header field is must on top
				auto header_block = _hpack_encoder->Encode({":status", ov::Converter::ToString(static_cast<uint16_t>(GetStatusCode()))}, hpack::Encoder::EncodingType::LiteralWithIndexing);
				response->Append(header_block);

				for (const auto &[name, values] : GetResponseHeaderList())
				{
					for (const auto &value : values)
					{
						// https://httpwg.org/http2-spec/draft-ietf-httpbis-http2bis.html#section-8.2
						// Field names MUST be converted to lowercase when constructing an HTTP/2 message.
						auto header_block = _hpack_encoder->Encode({name.LowerCaseString(), value}, hpack::Encoder::EncodingType::LiteralWithIndexing);
						response->Append(header_block);

						logtw("Send Headers : [%s] %s", name.CStr(), value.CStr());

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
				if (_keep_stream == false && GetResponseDataSize() == 0)
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

			uint32_t Http2Response::SendPayload()
			{
				logtd("Trying to send datas...");

				uint32_t sent_bytes = 0;

				for (const auto &data : GetResponseDataList())
				{
					size_t offset = 0;
					auto data_fragment = data;
					while (offset + MAX_HTTP2_DATA_SIZE < data->GetLength())
					{
						data_fragment = data->Subdata(offset, MAX_HTTP2_DATA_SIZE);

						auto payload_frame = std::make_shared<prot::h2::Http2DataFrame>(_stream_id);
						payload_frame->SetData(data_fragment);

						if (Send(payload_frame) == false)
						{
							logte("Failed to send payload");
							ResetResponseData();
							return -1;
						}

						offset += MAX_HTTP2_DATA_SIZE;
					}

					// Last fragment
					auto payload_frame = std::make_shared<prot::h2::Http2DataFrame>(_stream_id);
					data_fragment = data->Subdata(offset);
					payload_frame->SetData(data_fragment);

					// End Stream
					if (_keep_stream == false && (&data == &GetResponseDataList().back()))
					{
						payload_frame->SetEndStream();
					}

					if (Send(payload_frame) == false)
					{
						logte("Failed to send payload");
						ResetResponseData();
						return -1;
					}

					sent_bytes += data->GetLength();
				}

				ResetResponseData();

				logtd("All datas are sent...");

				return sent_bytes;
			}

		} // namespace h2
	} // namespace svr
} // namespace http