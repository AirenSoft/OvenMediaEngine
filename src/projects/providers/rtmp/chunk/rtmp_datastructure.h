//==============================================================================
//
//  RtmpProvider
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

enum class RtmpChunkType : uint8_t
{
	T0 = 0b00000000,
	T1 = 0b01000000,
	T2 = 0b10000000,
	T3 = 0b11000000
};

#pragma pack(push, 1)
struct RtmpChunkHeader
{
	RtmpChunkHeader()
	{
		::memset(&header, 0, sizeof(header));
	}

	// +--------------+----------------+--------------------+--------------+
	// | Basic Header | Message Header | Extended Timestamp |  Chunk Data  |
	// +--------------+----------------+--------------------+--------------+
	// |                                                    |
	// |<------------------- Chunk Header ----------------->|
	//                             Chunk Format

	// Indicates whether the timestamp is extended
	bool is_extended = false;

	// Total chunk header size (basic header + message header + extended timestamp)
	uint32_t header_size = 0U;

	void RecalculateHeaderSize()
	{
		header_size = basic_header_size + message_header_size + ((is_extended) ? sizeof(uint32_t) : 0U);
	}

	uint32_t basic_header_size = 0U;
	uint32_t message_header_size = 0U;
	// The payload size that NOT including type 3 messages
	uint32_t payload_size = 0U;

	uint32_t expected_type_3_header{};
	// The payload size that including type 3 messages
	uint32_t expected_payload_size = 0U;

	// Basic Header
	struct
	{
		// Chunk type
		RtmpChunkType format_type;

		// Chunk stream id
		uint32_t stream_id;
	} basic_header;

	// Extended Timestamp
	uint32_t extended_timestamp = 0U;

	struct
	{
		// Accumulated timestamp
		uint32_t timestamp_delta = 0U;
		int64_t timestamp = 0LL;
		uint32_t length = 0U;
		uint8_t type_id = 0U;
		uint32_t stream_id = 0U;
	} completed;

	// Message Header
	union
	{
		// Type 0 chunk headers are 11 bytes long. This type MUST be used at
		// the start of a chunk stream, and whenever the stream timestamp goes
		// backward (e.g., because of a backward seek).
		//
		//  0                   1                   2                   3
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |                   timestamp                   |message length |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |     message length (cont)     |message type id| msg stream id |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |           message stream id (cont)            |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//
		//                   Chunk Message Header - Type 0
		struct
		{
			uint32_t timestamp : 24;
			uint32_t length : 24;
			uint8_t type_id;
			uint32_t stream_id;
		} type_0;

		// Type 1 chunk headers are 7 bytes long. The message stream ID is not
		// included; this chunk takes the same stream ID as the preceding chunk.
		// Streams with variable-sized messages (for example, many video
		// formats) SHOULD use this format for the first chunk of each new
		// message after the first.
		//
		//  0                   1                   2                   3
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |                timestamp delta                |message length |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |     message length (cont)     |message type id|
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//
		//                   Chunk Message Header - Type 1
		struct
		{
			uint32_t timestamp_delta : 24;
			uint32_t length : 24;
			uint8_t type_id;
		} type_1;

		// Type 2 chunk headers are 3 bytes long. Neither the stream ID nor the
		// message length is included; this chunk has the same stream ID and
		// message length as the preceding chunk. Streams with constant-sized
		// messages (for example, some audio and data formats) SHOULD use this
		// format for the first chunk of each message after the first.
		//
		//  0                   1                   2                   3
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |                timestamp delta                |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//
		//           Chunk Message Header - Type 2
		struct
		{
			uint32_t timestamp_delta : 24;
		} type_2;

		// Type 3 chunks have no message header. The stream ID, message length
		// and timestamp delta fields are not present; chunks of this type take
		// values from the preceding chunk for the same Chunk Stream ID. When a
		// single message is split into chunks, all chunks of a message except
		// the first one SHOULD use this type.
		struct
		{
		} type_3;
	} header;

	ov::String ToString() const
	{
		ov::String result = ov::String::FormatString("<RtmpChunkHeader: %p, Basic: ", this);

		if (basic_header_size > 0U)
		{
			result.AppendFormat("<Size: %d, Type: %d, StreamID: %u, TS: %lld>, Message: ",
								basic_header_size,
								static_cast<int>(basic_header.format_type) >> 6,
								basic_header.stream_id,
								completed.timestamp
								);

			if ((basic_header.format_type == RtmpChunkType::T3) || (message_header_size > 0U))
			{
				switch (basic_header.format_type)
				{
					case RtmpChunkType::T0:
						result.AppendFormat("<Type0: TS: %u, Len: %u bytes, TypeID: %d, StreamID: %d>",
											header.type_0.timestamp,
											header.type_0.length,
											header.type_0.type_id,
											header.type_0.stream_id);
						break;

					case RtmpChunkType::T1:
						result.AppendFormat("<Type1: dTS: %u, Len: %u bytes, TypeID: %d>",
											header.type_1.timestamp_delta,
											header.type_1.length,
											header.type_1.type_id);
						break;

					case RtmpChunkType::T2:
						result.AppendFormat("<Type2: dTS: %u>",
											header.type_2.timestamp_delta);
						break;

					case RtmpChunkType::T3:
						result.AppendFormat("<Type3>");
						break;
				}

				if (is_extended)
				{
					result.AppendFormat(", Extended TS: %u", extended_timestamp);
				}

				result.AppendFormat(", Payload: %u bytes (With Type 3: %u)", payload_size, expected_payload_size);
			}
			else
			{
				result.Append("Not parsed");
			}
		}
		else
		{
			result.Append("Not parsed");
		}

		result.Append('>');

		return std::move(result);
	}
};
#pragma pack(pop)

struct RtmpMessage
{
public:
	RtmpMessage(const std::shared_ptr<const RtmpChunkHeader> &header, const std::shared_ptr<const ov::Data> &payload)
		: header(header),
		  payload(payload)
	{
	}

	std::shared_ptr<const RtmpChunkHeader> header;
	std::shared_ptr<const ov::Data> payload;
};
