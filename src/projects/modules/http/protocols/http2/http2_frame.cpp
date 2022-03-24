//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http2_frame.h"

namespace http
{
	namespace svr
	{
		ssize_t Http2Frame::AppendData(const std::shared_ptr<const ov::Data> &data)
		{
			ssize_t consumed_bytes = 0;
			auto input_data = data;

			if (_state == Http2Frame::State::Init)
			{
				// Need Haeder Data
				auto need_header_bytes = HTTP2_FRAME_HEADER_SIZE - _packet_data.GetLength();
				if (input_data->GetLength() >= need_header_bytes)
				{
					consumed_bytes += need_header_bytes;
					_packet_data.Append(input_data->GetData(), need_header_bytes);
					input_data = input_data->Subdata(need_header_bytes);
					
					// Parse Header
					if (ParseHeader() == false)
					{
						_state = Http2Frame::State::Error;
						return -1;
					}
				}
				else
				{
					consumed_bytes += input_data->GetLength();
					_packet_data.Append(input_data);
					// Need more data
				}
			}

			if (_state == Http2Frame::State::HeaderParsed)
			{
				// Need Length Data
				auto need_payload_bytes = GetLength() - (_packet_data.GetLength() - HTTP2_FRAME_HEADER_SIZE);
				if (input_data->GetLength() >= need_payload_bytes)
				{
					consumed_bytes += need_payload_bytes;
					_packet_data.Append(input_data->GetData(), need_payload_bytes);
				}
				else
				{
					consumed_bytes += input_data->GetLength();
					_packet_data.Append(input_data);
					
					// Need more data to complete Frame
				}

				if (_packet_data.GetLength() - HTTP2_FRAME_HEADER_SIZE == GetLength())
				{
					_payload = _packet_data.Subdata(HTTP2_FRAME_HEADER_SIZE);
					_state = Http2Frame::State::Completed;	
				}
			}

			return consumed_bytes;
		}

		bool Http2Frame::ParseHeader()
		{
			if (_packet_data.GetLength() < HTTP2_FRAME_HEADER_SIZE)
			{
				return false;
			}

			auto header_data = _packet_data.GetWritableDataAs<uint8_t>();

			// Parse HTTP/2 Frame Length (24bit)
			

			// Parse Length
			_length = (header_data[0] << 16) | (header_data[1] << 8) | header_data[2];

			// Parse Type
			_type = static_cast<Type>(header_data[3]);

			// Parse Flags
			_flags = static_cast<Flags>(header_data[4]);

			
			// R: A reserved 1-bit field.  The semantics of this bit are undefined,
			// and the bit MUST remain unset (0x0) when sending and MUST be ignored when receiving.

			// Check reserved bit of Stream Identifier is 0
			// if ((header_data[5] & 0x80) != 0)
			// {
			// 	// Set reserved bit of Stream Identifier to 0
			// 	header_data[5] &= 0x7F;
			//
			// 	// Or failed
			// 	return false;
			// }

			// Parse Stream ID - ignore reserved bit
			_stream_id = (header_data[5] << 24) | (header_data[6] << 16) | (header_data[7] << 8) | header_data[8];

			_state = Http2Frame::State::HeaderParsed;

			return true;
		}

		ov::String Http2Frame::ToString() const
		{
			return ov::String::FormatString(
				"<Http2Frame: %p, Type: %d, Flags: %d, Stream ID: %d, Payload Length: %ld",
				this, GetType(), GetFlags(), GetStreamId(), GetLength());
		}

		Http2Frame::State Http2Frame::GetState() const noexcept
		{
			return _state;
		}

		uint32_t Http2Frame::GetLength() const
		{
			return _length;
		}

		Http2Frame::Type Http2Frame::GetType() const noexcept
		{
			return _type;
		}

		Http2Frame::Flags Http2Frame::GetFlags() const noexcept
		{
			return _flags;
		}

		uint32_t Http2Frame::GetStreamId() const noexcept
		{
			return _stream_id;
		}

		const std::shared_ptr<const ov::Data> Http2Frame::GetPayload() const noexcept
		{
			return _payload;
		}
	}  // namespace svr
}  // namespace http