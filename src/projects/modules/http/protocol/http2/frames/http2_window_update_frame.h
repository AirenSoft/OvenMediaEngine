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
			class Http2WindowUpdateFrame : public Http2Frame
			{
			public:
				// Make from parsed general frame header and payload
				Http2WindowUpdateFrame(const std::shared_ptr<const Http2Frame> &frame)
					: Http2Frame(frame)
				{
					ParsingPayload();
				}

				// Make by itself
				Http2WindowUpdateFrame()
				{
					SetType(Http2Frame::Type::WindowUpdate);
					// Unused flags
					SetFlags(0);
				}

				// Get Window Size Increment
				uint32_t GetWindowSizeIncrement() const
				{
					return _window_size_increment;
				}

				// Set Window Size Increment
				bool SetWindowSizeIncrement(uint32_t window_size_increment)
				{
					// can't be 0  and can't be larger than 2^31-1 (RFC 7540 Section 6.9)
					OV_ASSERT2(window_size_increment > 0 && window_size_increment <= 0x7FFFFFFF);
					
					if (window_size_increment > 0 && window_size_increment <= 0x7FFFFFFF)
					{
						_window_size_increment = window_size_increment;
						return true;
					}

					return false;
				}

				// To String
				ov::String ToString() const override
				{
					ov::String str;
					
					str = Http2Frame::ToString();

					str += "\n";
					str += "[WindowUpdate Frame]\n";
					str += ov::String::FormatString("Window size increment: %u\n", GetWindowSizeIncrement());
				
					return str;
				}

				// To data
				const std::shared_ptr<const ov::Data> GetPayload() const override
				{
					if (Http2Frame::GetPayload() != nullptr)
					{
						return Http2Frame::GetPayload();
					}

					auto payload = std::make_shared<ov::Data>();
					ov::ByteStream stream(payload.get()); 
					stream.WriteBE32(_window_size_increment);

					return payload;
				}

			private:
				void ParsingPayload()
				{
					// WindowUpdate frame must have a payload of at least 4 bytes
					auto payload = GetPayload();
					if (payload == nullptr)
					{
						SetParsingState(ParsingState::Error);
						return;
					}
					
					// Parse each parameter in the payload
					auto payload_data = payload->GetDataAs<uint8_t>();
					auto payload_size = payload->GetLength();

					if (payload_size != 4)
					{
						SetParsingState(ParsingState::Error);
						return;
					}

					_window_size_increment = ByteReader<uint32_t>::ReadBigEndian(payload_data);
					
					SetParsingState(ParsingState::Completed);
				}

				std::shared_ptr<Http2Frame> _frame;
				
				uint32_t _window_size_increment = 983041; // Default value
			};
		}
	}
}