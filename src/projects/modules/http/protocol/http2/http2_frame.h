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
namespace http
{
	namespace prot
	{
		namespace h2
		{

			// https://datatracker.ietf.org/doc/html/rfc7540#section-4.1
			//
			// +-----------------------------------------------+
			// |                 Length (24)                   |
			// +---------------+---------------+---------------+
			// |   Type (8)    |   Flags (8)   |
			// +-+-------------+---------------+-------------------------------+
			// |R|                 Stream Identifier (31)                      |
			// +=+=============================================================+
			// |                   Frame Payload (0...)                      ...
			// +---------------------------------------------------------------+
			//                       Figure 1: Frame Layout

#define HTTP2_FRAME_HEADER_SIZE 3 + 1 + 1 + 4

			// For HTTP/2.0
			class Http2Frame
			{
			public:
				enum class State : uint8_t
				{
					Init,
					HeaderParsed,
					Completed,
					Error,
				};

				enum class Type : uint8_t
				{
					Data = 0x0,
					Headers = 0x1,
					Priority = 0x2,
					RstStream = 0x3,
					Settings = 0x4,
					PushPromise = 0x5,
					Ping = 0x6,
					GoAway = 0x7,
					WindowUpdate = 0x8,
					Continuation = 0x9,
					Unknown = 0xA,
				};

				enum class Flags : uint8_t
				{
					None = 0x0,
					EndStream = 0x1,
					EndHeaders = 0x4,
					Padded = 0x8,
					Priority = 0x20,
				};

				ssize_t AppendData(const std::shared_ptr<const ov::Data> &data);

				ov::String ToString() const;

				// Getters
				State GetState() const noexcept;
				uint32_t GetLength() const;
				Type GetType() const noexcept;
				Flags GetFlags() const noexcept;
				uint32_t GetStreamId() const noexcept;
				const std::shared_ptr<const ov::Data> GetPayload() const noexcept;

			private:
				bool ParseHeader();

				State _state = State::Init;

				uint32_t _length = 0;
				Type _type = Type::Unknown;
				Flags _flags = Flags::None;
				uint32_t _stream_id = 0;
				std::shared_ptr<const ov::Data> _payload = nullptr;
				ov::Data _packet_data;
			};
		}  // namespace h2
	} // namespace prot
}  // namespace http