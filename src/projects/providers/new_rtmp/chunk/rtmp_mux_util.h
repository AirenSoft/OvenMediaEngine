//==============================================================================
//
//  RtmpProvider
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "rtmp_define.h"
#pragma pack(1)
//====================================================================================================
// Mux 메시지 헤더
//====================================================================================================
struct RtmpMuxMessageHeader
{
public:
	RtmpMuxMessageHeader()
		: chunk_stream_id(0),
		  timestamp(0),
		  type_id(0),
		  stream_id(0),
		  body_size(0)
	{
	}

	RtmpMuxMessageHeader(uint32_t chunk_stream_id_,
						 uint32_t timestamp_,
						 uint8_t type_id_,
						 uint32_t stream_id_,
						 uint32_t body_size_)
	{
		this->chunk_stream_id = chunk_stream_id_;
		this->timestamp = timestamp_;
		this->type_id = type_id_;
		this->stream_id = stream_id_;
		this->body_size = body_size_;
	}

public:
	uint32_t chunk_stream_id;
	uint32_t timestamp;
	uint8_t type_id;
	uint32_t stream_id;
	uint32_t body_size;
};
#pragma pack()

class RtmpMuxUtil
{
public:
	RtmpMuxUtil() = default;
	virtual ~RtmpMuxUtil() = default;

public:
	static uint8_t ReadInt8(const void *data);
	static uint16_t ReadInt16(const void *data);
	static uint32_t ReadInt24(const void *data);
	static uint32_t ReadInt32(const void *data);
	static int ReadInt32LE(const void *data);

	static int WriteInt8(void *output, uint8_t nVal);
	static int WriteInt16(void *output, int16_t nVal);
	static int WriteInt24(void *output, int nVal);
	static int WriteInt32(void *output, int nVal);
	static int WriteInt32LE(void *output, int nVal);

	static int GetBasicHeaderSizeByRawData(uint8_t data) noexcept;
	static int GetBasicHeaderSizeByChunkStreamID(uint32_t chunk_stream_id) noexcept;

	static int GetChunkHeaderSize(RtmpChunkType chunk_type, uint32_t chunk_stream_id, int basic_header_size, void *raw_data, int raw_data_size);  // ret:길이, ret<=0:실패
	static std::shared_ptr<RtmpChunkHeader> GetChunkHeader(void *raw_data, int raw_data_size, int &chunk_header_size, bool &extend_type);		  // ret<=0:실패, ret>0:처리길이
	static int GetChunkData(int chunk_size, void *raw_data, int raw_data_size, int chunk_data_size, void *chunk_data, bool extend_type);		  // ret=> 0:실패, 1:처리길이

	static int GetChunkBasicHeaderRaw(RtmpChunkType chunk_type, uint32_t chunk_stream_id, void *raw_data);
	static int GetChunkHeaderRaw(std::shared_ptr<RtmpChunkHeader> &chunk_header, void *raw_data, bool extend_type);
	static int GetChunkDataRawSize(int chunk_size, uint32_t chunk_stream_id, int chunk_data_size, bool extend_type);
	static int GetChunkDataRaw(int chunk_size, uint32_t chunk_stream_id, std::shared_ptr<std::vector<uint8_t>> &chunk_data, void *raw_data, bool extend_type, uint32_t time);
};
