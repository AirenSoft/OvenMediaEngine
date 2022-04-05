//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/byte_io.h>
#include "../http2_frame.h"

namespace http
{
	namespace prot
	{
		namespace h2
		{
			class Http2RstStreamFrame : public Http2Frame
			{
			public:
				// Make by itself
				Http2RstStreamFrame()
					// https://datatracker.ietf.org/doc/html/rfc7540#section-6.5
					// SETTINGS frames always apply to a connection, never a single stream. 
					// The stream identifier for a SETTINGS frame MUST be zero (0x00).
					: Http2Frame(0)
				{
					SetType(Http2Frame::Type::GoAway);
					// Settings Frame's stream id is 0
					SetStreamId(0);
				}

				Http2RstStreamFrame(const std::shared_ptr<Http2Frame> &frame)
					: Http2Frame(frame)
				{
				}

				// Setters
				void SetErrorCode(uint32_t error_code)
				{
					_error_code = error_code;
				}

				// Getters
				uint32_t GetErrorCode() const
				{
					return _error_code;
				}

				// To String
				ov::String ToString() const override
				{
					ov::String str;
					
					str = Http2Frame::ToString();

					str += "\n";
					str += "[RST_STREAM Frame]\n";

					// Print parameters
					str += ov::String::FormatString("Error Code : %u\n", _error_code);

					return str;
				}

				// To data
				const std::shared_ptr<const ov::Data> GetPayload() const override
				{
					if (Http2Frame::GetPayload() != nullptr)
					{
						return Http2Frame::GetPayload();
					}

					auto payload = std::make_shared<ov::Data>(4);
					ov::ByteStream stream(payload.get()); 

					stream.WriteBE32(_error_code);

					return payload;
				}

			private:
				bool ParsePayload() override
				{
					if (GetType() != Http2Frame::Type::GoAway)
					{
						return false;
					}

					// The payload of a SETTINGS frame consists of zero or more parameters,
					auto payload = GetPayload();
					if (payload == nullptr)
					{
						SetParsingState(ParsingState::Completed);
						return false;
					}

                    // Check payload size
                    if (payload->GetLength() < 4)
                    {
                        SetParsingState(ParsingState::Error);
                        return false;
                    }
					
					// Parse each parameter in the payload
					auto payload_data = payload->GetDataAs<uint8_t>();
					size_t payload_offset = 0;

					_error_code = ByteReader<uint32_t>::ReadBigEndian(payload_data + payload_offset);
					payload_offset += sizeof(uint32_t);

					SetParsingState(ParsingState::Completed);

					return true;
				}

				uint32_t _error_code = 0;
			};
		}
	}
}