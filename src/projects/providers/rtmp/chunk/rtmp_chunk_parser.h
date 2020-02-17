//==============================================================================
//
//  RtmpProvider
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtmp_define.h"

#include <memory>

#include <base/ovlibrary/ovlibrary.h>

class RtmpChunkParser
{
public:
	// Return values:
	//
	// <0: An error occurred while parsing
	//  0: Need more data
	// >0: Parsing is completed (The header & the payload are available)
	off_t Parse(const std::map<uint32_t, std::shared_ptr<const RtmpChunkHeader>> &chunk_map, ov::ByteStream &stream);

	bool IsParseCompleted() const noexcept
	{
		return (_parse_status == ParseStatus::Completed);
	}

	bool Reset()
	{
		_current_chunk_header = nullptr;
		_parse_status = ParseStatus::BasicHeader;

		return true;
	}

	std::shared_ptr<RtmpChunkHeader> GetParsedChunkHeader()
	{
		if (IsParseCompleted())
		{
			return _current_chunk_header;
		}

		return nullptr;
	}

	static RtmpChunkType GetChunkType(uint8_t first_byte);
	static int GetBasicHeaderSize(uint8_t first_byte);

protected:
	// Use ParseResult to distinguish between a header with a length of zero and a situation with insufficient data
	enum class ParseResult
	{
		NeedMoreData,
		Completed,
		Failed
	};

	enum class ParseStatus
	{
		BasicHeader,
		MessageHeader,
		ExtendedTimestamp,
		Completed
	};

	ParseResult ParseBasicHeader(ov::ByteStream &stream, off_t *parsed_bytes);
	ParseResult ParseMessageHeader(ov::ByteStream &stream, off_t *parsed_bytes);
	ParseResult ParseExtendedTimestamp(std::shared_ptr<const RtmpChunkHeader> last_chunk, ov::ByteStream &stream, off_t *parsed_bytes);
	ParseResult ParsePayload(ov::ByteStream &stream, off_t *parsed_bytes);

	std::shared_ptr<RtmpChunkHeader> _current_chunk_header;

	ParseStatus _parse_status = ParseStatus::BasicHeader;
};