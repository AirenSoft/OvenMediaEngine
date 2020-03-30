//==============================================================================
//
//  RtmpProvider
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "rtmp_chunk_parser.h"
#include "providers/rtmp/rtmp_provider_private.h"

off_t RtmpChunkParser::Parse(const std::map<uint32_t, std::shared_ptr<const RtmpChunkHeader>> &chunk_map, ov::ByteStream &stream)
{
	off_t total_parsed_bytes = 0LL;
	off_t parsed_bytes = 0LL;

	while (stream.IsEmpty() == false)
	{
		switch (_parse_status)
		{
			case ParseStatus::BasicHeader:
				logtd("Trying to parse basic header...");

				_current_chunk_header = std::make_shared<RtmpChunkHeader>();
				total_parsed_bytes = 0LL;

				switch (ParseBasicHeader(stream, &parsed_bytes))
				{
					case ParseResult::NeedMoreData:
						logtd("Waiting for more data to parse (%jd bytes used)", total_parsed_bytes);
						return total_parsed_bytes;

					case ParseResult::Completed:
						_parse_status = ParseStatus::MessageHeader;
						total_parsed_bytes += parsed_bytes;
						logtd("RTMP chunk is parsed (%jd bytes used)", total_parsed_bytes);
						break;

					case ParseResult::Failed:
						return -1LL;
				}

				break;

			case ParseStatus::MessageHeader:
				switch (ParseMessageHeader(stream, &parsed_bytes))
				{
					case ParseResult::NeedMoreData:
						return total_parsed_bytes;

					case ParseResult::Completed:
						_parse_status = ParseStatus::ExtendedTimestamp;
						total_parsed_bytes += parsed_bytes;
						break;

					case ParseResult::Failed:
						return -1LL;
				}

				break;

			case ParseStatus::ExtendedTimestamp:
			{
				auto last_chunk_item = chunk_map.find(_current_chunk_header->basic_header.stream_id);
				std::shared_ptr<const RtmpChunkHeader> last_chunk;

				if (last_chunk_item != chunk_map.end())
				{
					last_chunk = last_chunk_item->second;
				}

				switch (ParseExtendedTimestamp(last_chunk, stream, &parsed_bytes))
				{
					case ParseResult::NeedMoreData:
						return total_parsed_bytes;

					case ParseResult::Completed:
						_parse_status = ParseStatus::Completed;
						total_parsed_bytes += parsed_bytes;
						_current_chunk_header->RecalculateHeaderSize();

						return total_parsed_bytes;

					case ParseResult::Failed:
						return -1LL;
				}

				break;
			}

			case ParseStatus::Completed:
				_parse_status = ParseStatus::BasicHeader;

				break;
		}
	}

	return total_parsed_bytes;
}

RtmpChunkType RtmpChunkParser::GetChunkType(uint8_t first_byte)
{
	auto chunk_type = static_cast<RtmpChunkType>(first_byte & 0b11000000);

	// Validate RTMP chunk type
	switch (chunk_type)
	{
		case RtmpChunkType::T0:
		case RtmpChunkType::T1:
		case RtmpChunkType::T2:
		case RtmpChunkType::T3:
			break;

		default:
			// It is 4 bits, so no other value can be obtained
			OV_ASSERT2(false);
	}

	return chunk_type;
}

int RtmpChunkParser::GetBasicHeaderSize(uint8_t first_byte)
{
	int chunk_stream_id = first_byte & 0b00111111;

	switch (chunk_stream_id)
	{
		case 0:
			// Value 0 indicates the 2 byte form and an ID in the range of 64-319
			// (the second byte + 64)
			return 2;

		case 1:
			// Value 1 indicates the 3 byte form and an ID in the range of 64-65599
			// ((the third byte)*256 + the second byte + 64)
			return 3;

		default:
			// Chunk stream IDs 2-63 can be encoded in the 1-byte version of this field

			if (chunk_stream_id == 2)
			{
				// Chunk Stream ID with value 2 is reserved for low-level protocol control messages and commands
			}
			else
			{
				// Values in the range of 3-63 represent the complete stream ID
			}

			return 1;
	}
}

RtmpChunkParser::ParseResult RtmpChunkParser::ParseBasicHeader(ov::ByteStream &stream, off_t *parsed_bytes)
{
	// The protocol supports up to 65597 streams with IDs 3-65599. The IDs
	// 0, 1, and 2 are reserved. Value 0 indicates the 2 byte form and an
	// ID in the range of 64-319 (the second byte + 64). Value 1 indicates
	// the 3 byte form and an ID in the range of 64-65599 ((the third
	// byte)*256 + the second byte + 64). Values in the range of 3-63
	// represent the complete stream ID. Chunk Stream ID with value 2 is
	// reserved for low-level protocol control messages and commands.
	//
	// The bits 0-5 (least significant) in the chunk basic header represent
	// the chunk stream ID.
	//
	// Chunk stream IDs 2-63 can be encoded in the 1-byte version of this
	// field.
	//
	//                            0 1 2 3 4 5 6 7
	//                           +-+-+-+-+-+-+-+-+
	//                           |fmt|   cs id   |
	//                           +-+-+-+-+-+-+-+-+
	//                         Chunk basic header 1
	//
	// Chunk stream IDs 64-319 can be encoded in the 2-byte form of the
	// header. ID is computed as (the second byte + 64).
	//
	//                    0                   1
	//                    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
	//                   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//                   |fmt|     0     |   cs id - 64  |
	//                   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//                         Chunk basic header 2
	//
	// Chunk stream IDs 64-65599 can be encoded in the 3-byte version of
	// this field. ID is computed as ((the third byte)*256 + (the second
	// byte) + 64).
	//
	//            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
	//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//           |fmt|     1     |        cs id - 64             |
	//           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//                         Chunk basic header 3

	// Parse chunk format & basic header size & parse chunk stream ID
	if (stream.IsRemained(1) == false)
	{
		return ParseResult::NeedMoreData;
	}
	auto first_byte = stream.Read8();

	_current_chunk_header->basic_header.format_type = GetChunkType(first_byte);
	_current_chunk_header->basic_header_size = GetBasicHeaderSize(first_byte);

	OV_ASSERT2(_current_chunk_header->basic_header_size > 0);

	if (stream.IsRemained((_current_chunk_header->basic_header_size - 1)) == false)
	{
		return ParseResult::NeedMoreData;
	}

	int chunk_stream_id = first_byte & 0b00111111;

	switch (_current_chunk_header->basic_header_size)
	{
		case 1:
			break;

		case 2:
			// stream_id = (the second byte + 64)
			chunk_stream_id = stream.Read8() + 64;

			break;

		case 3:
			// stream_id = ((the third byte) * 256 + (the second byte) + 64)
			chunk_stream_id = stream.Read8() + 64;
			chunk_stream_id += stream.Read8() * 256;
			break;
	}

	OV_ASSERT2(stream.GetOffset() == _current_chunk_header->basic_header_size);

	_current_chunk_header->basic_header.stream_id = chunk_stream_id;
	*parsed_bytes = _current_chunk_header->basic_header_size;

	return ParseResult::Completed;
}

RtmpChunkParser::ParseResult RtmpChunkParser::ParseMessageHeader(ov::ByteStream &stream, off_t *parsed_bytes)
{
#if DEBUG
	off_t current_offset = stream.GetOffset();
#endif  // DEBUG

	// Obtain header size to move the offset of raw_data_pos
	switch (_current_chunk_header->basic_header.format_type)
	{
		case RtmpChunkType::T0:
			_current_chunk_header->message_header_size = sizeof(RtmpChunkHeader::header.type_0);

			if (stream.IsRemained(_current_chunk_header->message_header_size) == false)
			{
				return ParseResult::NeedMoreData;
			}

			_current_chunk_header->header.type_0.timestamp = stream.ReadBE24();
			_current_chunk_header->header.type_0.length = stream.ReadBE24();
			_current_chunk_header->header.type_0.type_id = stream.Read8();
			_current_chunk_header->header.type_0.stream_id = stream.ReadLE32();

			_current_chunk_header->payload_size = _current_chunk_header->header.type_0.length;

			OV_ASSERT((stream.GetOffset() - current_offset) == _current_chunk_header->message_header_size, "Expected: %lld, But: %lld", _current_chunk_header->message_header_size, (stream.GetOffset() - current_offset));

			break;

		case RtmpChunkType::T1:
			_current_chunk_header->message_header_size = sizeof(RtmpChunkHeader::header.type_1);

			if (stream.IsRemained(_current_chunk_header->message_header_size) == false)
			{
				return ParseResult::NeedMoreData;
			}

			_current_chunk_header->header.type_1.timestamp_delta = stream.ReadBE24();
			_current_chunk_header->header.type_1.length = stream.ReadBE24();
			_current_chunk_header->header.type_1.type_id = stream.Read8();

			_current_chunk_header->payload_size = _current_chunk_header->header.type_1.length;

			OV_ASSERT((stream.GetOffset() - current_offset) == _current_chunk_header->message_header_size, "Expected: %lld, But: %lld", _current_chunk_header->message_header_size, (stream.GetOffset() - current_offset));

			break;

		case RtmpChunkType::T2:
			_current_chunk_header->message_header_size = sizeof(RtmpChunkHeader::header.type_2);

			if (stream.IsRemained(_current_chunk_header->message_header_size) == false)
			{
				return ParseResult::NeedMoreData;
			}

			_current_chunk_header->header.type_2.timestamp_delta = stream.ReadBE24();

			// Neither the stream ID nor the message length is included;
			// this chunk has the same stream ID and message length as the preceding chunk.
			_current_chunk_header->payload_size = 0LL;

			OV_ASSERT((stream.GetOffset() - current_offset) == _current_chunk_header->message_header_size, "Expected: %lld, But: %lld", _current_chunk_header->message_header_size, (stream.GetOffset() - current_offset));

			break;

		case RtmpChunkType::T3:
			// Type 3 hasn't a header

			// The stream ID, message length and timestamp delta fields are not present;
			// chunks of this type take values from the preceding chunk for the same Chunk Stream ID
			_current_chunk_header->payload_size = 0LL;

			break;

		default:
			OV_ASSERT2(false);
			return ParseResult::Failed;
	}

	*parsed_bytes = _current_chunk_header->message_header_size;
	return ParseResult::Completed;
}

RtmpChunkParser::ParseResult RtmpChunkParser::ParseExtendedTimestamp(std::shared_ptr<const RtmpChunkHeader> last_chunk, ov::ByteStream &stream, off_t *parsed_bytes)
{
	uint32_t target_timestamp = 0U;

	switch (_current_chunk_header->basic_header.format_type)
	{
		case RtmpChunkType::T0:
			target_timestamp = _current_chunk_header->header.type_0.timestamp;
			break;

		case RtmpChunkType::T1:
			target_timestamp = _current_chunk_header->header.type_1.timestamp_delta;
			break;

		case RtmpChunkType::T2:
			target_timestamp = _current_chunk_header->header.type_2.timestamp_delta;
			break;

		case RtmpChunkType::T3:
			// Type 3 hasn't a header
			if ((last_chunk != nullptr) && (last_chunk->is_extended))
			{
				// Need to parse extended timestamp
				target_timestamp = RTMP_EXTEND_TIMESTAMP;
				break;
			}

			return ParseResult::Completed;

		default:
			OV_ASSERT2(false);
			return ParseResult::Failed;
	}

	if (target_timestamp == RTMP_EXTEND_TIMESTAMP)
	{
		if (stream.IsRemained(sizeof(RtmpChunkHeader::extended_timestamp)) == false)
		{
			return ParseResult::NeedMoreData;
		}

		_current_chunk_header->extended_timestamp = stream.ReadBE32();
		_current_chunk_header->is_extended = true;

		*parsed_bytes = sizeof(RtmpChunkHeader::extended_timestamp);
	}
	else
	{
		*parsed_bytes = 0LL;
	}

	return ParseResult::Completed;
}
