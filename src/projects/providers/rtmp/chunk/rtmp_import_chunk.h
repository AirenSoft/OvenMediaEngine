//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <deque>
#include <map>
#include <memory>

#include "rtmp_chunk_parser.h"
#include "rtmp_define.h"
#include "rtmp_mux_util.h"
#include "rtmp_datastructure.h"

#include <base/info/info.h>

class RtmpImportChunk : public RtmpMuxUtil
{
public:
	RtmpImportChunk(int chunk_size);
	~RtmpImportChunk() override;

	int Import(const std::shared_ptr<const ov::Data> &data, bool *is_completed);

	std::shared_ptr<const RtmpMessage> GetMessage();
	size_t GetMessageCount() const;

	void SetChunkSize(size_t chunk_size)
	{
		_chunk_size = chunk_size;
	}

	void SetAppName(const info::VHostAppName &app_name);
	void SetStreamName(const ov::String &stream_name);

	void Destroy();

private:
	int64_t CalculateRolledTimestamp(int64_t last_timestamp, int64_t parsed_timestamp);

	bool ProcessChunkHeader(const std::shared_ptr<RtmpChunkHeader> &chunk_header, const std::shared_ptr<const RtmpChunkHeader> &last_chunk_header);
	bool CalculateForType3Header(const std::shared_ptr<RtmpChunkHeader> &chunk_header);
	std::shared_ptr<const RtmpMessage> FinalizeMessage(const std::shared_ptr<const RtmpChunkHeader> &chunk_header, ov::ByteStream &stream);

	std::map<uint32_t, std::shared_ptr<const RtmpChunkHeader>> _chunk_map;
	ov::Queue<std::shared_ptr<const RtmpMessage>> _message_queue { nullptr, 500 };
	size_t _chunk_size;

	RtmpChunkParser _parser;

	info::VHostAppName _vhost_app_name;
	ov::String _stream_name;
};
