//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "rtmp_import_chunk.h"

#include "../rtmp_chunk_log_define.h"
#include "rtmp_chunk_parser.h"

RtmpImportChunk::RtmpImportChunk(int chunk_size)
{
	_chunk_size = chunk_size;
}

RtmpImportChunk::~RtmpImportChunk()
{
	Destroy();
}

int RtmpImportChunk::Import(const std::shared_ptr<const ov::Data> &data, bool *is_completed)
{
	off_t parsed_bytes = 0LL;
	std::shared_ptr<const RtmpChunkHeader> last_chunk_header;

	*is_completed = false;

	ov::ByteStream stream(data.get());

	if (_parser.IsParseCompleted() == false)
	{
		parsed_bytes = _parser.Parse(stream);

		if (parsed_bytes < 0LL)
		{
			// An error occurred
			return static_cast<int>(parsed_bytes);
		}
		else if (_parser.IsParseCompleted() == false)
		{
			// Need more data
			OV_ASSERT2(parsed_bytes >= 0);

			return static_cast<int>(parsed_bytes);
		}

		// Need to parse the payload
		auto chunk_header = _parser.GetParsedChunkHeader();

		if (chunk_header == nullptr)
		{
			// chunk_header cannot be nullptr
			OV_ASSERT2(false);
			return -1LL;
		}

		auto item = _chunk_map.find(chunk_header->basic_header.stream_id);

		if (item != _chunk_map.end())
		{
			last_chunk_header = item->second;
		}
		else
		{
			// This is first chunk
		}

		if (ProcessChunkHeader(chunk_header, last_chunk_header) == false)
		{
			return -1LL;
		}

		_chunk_map[chunk_header->basic_header.stream_id] = chunk_header;
		last_chunk_header = std::move(chunk_header);
	}
	else
	{
		// Try to parse the payload using last chunk header
		auto chunk_header = _parser.GetParsedChunkHeader();

		if (chunk_header == nullptr)
		{
			// chunk_header cannot be nullptr
			OV_ASSERT2(false);
			return -1LL;
		}

		auto item = _chunk_map.find(chunk_header->basic_header.stream_id);

		if (item == _chunk_map.end())
		{
			logte("Could not find stream for stream_id: %u", chunk_header->basic_header.stream_id);
			return -1LL;
		}

		last_chunk_header = item->second;
	}

	if (stream.IsRemained(last_chunk_header->expected_payload_size) == false)
	{
		// Need more data
		OV_ASSERT2(parsed_bytes >= 0);

		return parsed_bytes;
	}

	auto message = FinalizeMessage(last_chunk_header, stream);

	if (message == nullptr)
	{
		return -1LL;
	}

	_message_queue.push_back(message);

	*is_completed = true;
	_parser.Reset();

	return parsed_bytes + last_chunk_header->expected_payload_size;
}

bool RtmpImportChunk::ProcessChunkHeader(const std::shared_ptr<RtmpChunkHeader> &chunk_header, const std::shared_ptr<const RtmpChunkHeader> &last_chunk_header)
{
	if ((last_chunk_header == nullptr) && (chunk_header->basic_header.format_type != RtmpChunkType::T0))
	{
		logte("Last chunk must be not null when incoming chunk is not first chunk (chunk type: %d)",
			  static_cast<int>(chunk_header->basic_header.format_type));
		return false;
	}

	auto &completed = chunk_header->completed;

	switch (chunk_header->basic_header.format_type)
	{
		case RtmpChunkType::T0:
		{
			auto &type_0 = chunk_header->header.type_0;

			completed.timestamp_delta = 0U;
			completed.timestamp = chunk_header->is_extended ? chunk_header->extended_timestamp : type_0.timestamp;

			completed.length = type_0.length;
			completed.type_id = type_0.type_id;
			completed.stream_id = type_0.stream_id;

			if (last_chunk_header != nullptr)
			{
				// Check if the timestamp is rolled
				auto last_timestamp = last_chunk_header->completed.timestamp;
				auto new_timestamp = completed.timestamp;

				if (last_timestamp > new_timestamp)
				{
					// RTMP specification
					//
					// Because timestamps are 32 bits long, they roll over every 49 days, 17
					// hours, 2 minutes and 47.296 seconds. Because streams are allowed to
					// run continuously, potentially for years on end, an RTMP application
					// SHOULD use serial number arithmetic [RFC1982] when processing
					// timestamps, and SHOULD be capable of handling wraparound. For
					// example, an application assumes that all adjacent timestamps are
					// within 2^31 - 1 milliseconds of each other, so 10000 comes after
					// 4000000000, and 3000000000 comes before 4000000000.

					// completed.timestamp calculated from this formula (https://tools.ietf.org/html/rfc1982#section-3.1):
					//
					// Serial numbers may be incremented by the addition of a positive
					// integer n, where n is taken from the range of integers
					// [0 .. (2^(SERIAL_BITS - 1) - 1)].  For a sequence number s, the
					// result of such an addition, s', is defined as
					//
					//                 s' = (s + n) modulo (2 ^ SERIAL_BITS)
					//
					// where the addition and modulus operations here act upon values that
					// are non-negative values of unbounded size in the usual ways of
					// integer arithmetic.

					// Check if the timestamp is an adjacent timestamp
					if (new_timestamp < ((1LL << 31) - 1))
					{
						// Adjacent timestamp

						completed.timestamp = last_timestamp + (1LL << 31) - (last_timestamp % (1LL << 31)) + new_timestamp;
					}
					else
					{
						// Non-adjacent timestamp
					}
				}
			}

			break;
		}

		case RtmpChunkType::T1:
		{
			auto &type_1 = chunk_header->header.type_1;

			// Copy chunk header from last_chunk_header
			completed = last_chunk_header->completed;

			completed.timestamp_delta = chunk_header->is_extended ? chunk_header->extended_timestamp : type_1.timestamp_delta;
			completed.timestamp += completed.timestamp_delta;
			completed.length = type_1.length;
			completed.type_id = type_1.type_id;

			break;
		}

		case RtmpChunkType::T2:
		{
			auto &type_2 = chunk_header->header.type_2;

			// Copy chunk header from last_chunk_header
			completed = last_chunk_header->completed;

			completed.timestamp_delta = chunk_header->is_extended ? chunk_header->extended_timestamp : type_2.timestamp_delta;
			completed.timestamp += completed.timestamp_delta;

			break;
		}

		case RtmpChunkType::T3:
		{
			// auto &type_3 = chunk_header->header.type_3;

			// Copy chunk header from last_chunk_header
			completed = last_chunk_header->completed;

			completed.timestamp += completed.timestamp_delta;

			break;
		}
	}

	chunk_header->message_header_size = sizeof(RtmpChunkHeader::completed);
	chunk_header->payload_size = chunk_header->completed.length;
	chunk_header->is_extended = (last_chunk_header != nullptr) ? last_chunk_header->is_extended : false;

	chunk_header->RecalculateHeaderSize();

	if (chunk_header->payload_size > RTMP_MAX_PACKET_SIZE)
	{
		logte("RTMP packet size is too large: %d (threshold: %d)", chunk_header->payload_size, RTMP_MAX_PACKET_SIZE);
		return false;
	}

	// When the transfered data larger than _chunk_size, type 3 header is in the middle
	if (CalculateForType3Header(chunk_header) == false)
	{
		logte("Could not calculate type 3 headers");
		return false;
	}

	OV_ASSERT2(chunk_header->basic_header_size >= 0);
	OV_ASSERT2(chunk_header->payload_size <= chunk_header->expected_payload_size);

	return true;
}

bool RtmpImportChunk::CalculateForType3Header(const std::shared_ptr<RtmpChunkHeader> &chunk_header)
{
	// In the middle, the chunk stream id of the type 3 message is the same as that of type 0
	uint8_t *expected_type_3_header = reinterpret_cast<uint8_t *>(&(chunk_header->expected_type_3_header));

	expected_type_3_header[0] = static_cast<uint32_t>(RtmpChunkType::T3);

	switch (chunk_header->basic_header_size)
	{
		case 1:
			// Chunk stream ID's range: 2-63
			expected_type_3_header[0] |= (chunk_header->basic_header.stream_id & 0b00111111);
			break;

		case 2:
			// Chunk stream ID's range: 64-319
			expected_type_3_header[0] |= 0b00000000;
			expected_type_3_header[1] = chunk_header->basic_header.stream_id - 64;
			break;

		case 3:
		{
			// Chunk stream ID's range: 64-65599
			auto stream_id = chunk_header->basic_header.stream_id - 64;

			expected_type_3_header[0] |= 0b00000001;
			expected_type_3_header[1] = stream_id & 0xFF;
			stream_id >>= 16;
			expected_type_3_header[2] = stream_id & 0xFF;
			break;
		}

		default:
			OV_ASSERT2(false);
			return false;
	}

	int type_3_count = (chunk_header->payload_size - 1) / _chunk_size;
	chunk_header->expected_payload_size = chunk_header->payload_size + (type_3_count * chunk_header->basic_header_size);

	OV_ASSERT2(type_3_count >= 0);

	return (type_3_count >= 0);
}

std::shared_ptr<const RtmpMessage> RtmpImportChunk::FinalizeMessage(const std::shared_ptr<const RtmpChunkHeader> &chunk_header, ov::ByteStream &stream)
{
	// We need to exclude the type 3 headers
	int index = 0;
	int basic_header_size = chunk_header->basic_header_size;
	size_t payload_size = chunk_header->expected_payload_size;
	const auto *expected_type_3_header = &(chunk_header->expected_type_3_header);
	auto payload_data = std::make_shared<ov::Data>(chunk_header->payload_size);
	const uint8_t *current = stream.GetRemainData()->GetDataAs<uint8_t>();

	while (payload_size > 0)
	{
		if (index > 0)
		{
			// Make sure that the message type of payload is type 3 and matches what was expected
			if (::memcmp(current, expected_type_3_header, basic_header_size) != 0)
			{
				logte("Invalid message is received: offset: %lld\nexpected:\n%s\nbut:\n%s",
					  (chunk_header->expected_payload_size - payload_size),
					  ov::Dump(expected_type_3_header, basic_header_size).CStr(),
					  ov::Dump(current, basic_header_size).CStr());

				return nullptr;
			}

			// skip type 3 header
			current += basic_header_size;
			payload_size -= basic_header_size;
		}

		size_t read_size = std::min(_chunk_size, payload_size);

		payload_data->Append(current, read_size);

		index++;

		current += read_size;
		payload_size -= read_size;
	}

	if (payload_data->GetLength() != chunk_header->payload_size)
	{
		OV_ASSERT(
			payload_data->GetLength() != chunk_header->payload_size,
			"Payload length: %zu, Expected: %zu", payload_data->GetLength(), chunk_header->payload_size);
		return nullptr;
	}

	auto message = std::make_shared<RtmpMessage>(chunk_header, std::move(payload_data));

	return std::move(message);
}

std::shared_ptr<const RtmpMessage> RtmpImportChunk::GetMessage()
{
	if (_message_queue.empty())
	{
		return nullptr;
	}

	auto message = _message_queue.front();
	_message_queue.pop_front();

	return message;
}

void RtmpImportChunk::Destroy()
{
	_chunk_map.clear();
	_message_queue.clear();

	_parser.Reset();
}