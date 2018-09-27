//==============================================================================
//
//  RtmpProvider
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include <memory>
#include <map>
#include <deque>
#include "rtmp_mux_util.h"

//====================================================================================================
// ImportStream
//====================================================================================================
struct ImportStream
{
public :
	ImportStream()
	{
		message_header 		= std::make_shared<RtmpMuxMessageHeader>();
        write_chunk_size	= 0;
        extend_type		= false;
        data_size			= 0;
        buffer 				= std::make_shared<std::vector<uint8_t>>(1024*1024);
	}

public :
    std::shared_ptr<RtmpMuxMessageHeader>	message_header;;
	uint32_t								timestamp_delta;
	int										write_chunk_size;
	bool									extend_type;
	int 									data_size;
	std::shared_ptr<std::vector<uint8_t>> 	buffer;
};

//====================================================================================================
// ImportMessage
//====================================================================================================
struct ImportMessage
{
public :
	ImportMessage(std::shared_ptr<RtmpMuxMessageHeader>	&header)
	{
        message_header  = header;
        body            = std::make_shared<std::vector<uint8_t>>(message_header->body_size);
	}

public :
    std::shared_ptr<RtmpMuxMessageHeader>	message_header;
	std::shared_ptr<std::vector<uint8_t>> 	body;
};

//====================================================================================================
// RtmpImportChunk
//====================================================================================================
class RtmpImportChunk : public RtmpMuxUtil
{
public:
	RtmpImportChunk(int chunk_size);
	~RtmpImportChunk() override;

public:
	void							        Destroy();
	int								        ImportStreamData(uint8_t *data, int data_size, bool & message_complete);
	std::shared_ptr<ImportMessage>          GetMessage();
	void                                    SetChunkSize(int chunk_size){ _chunk_size = chunk_size;}
private:
	std::shared_ptr<ImportStream>           GetStream(uint32_t chunk_stream_id);
	std::shared_ptr<RtmpMuxMessageHeader>   GetMessageHeader(std::shared_ptr<ImportStream> &stream, std::shared_ptr<RtmpChunkHeader> &chunk_header);
	bool							        AppendChunk(std::shared_ptr<ImportStream> &stream, uint8_t *chunk, int header_size, int data_size);
	int								        CompleteChunkMessage(std::shared_ptr<ImportStream> &stream, int chunk_size);

private:
	std::map<uint32_t, std::shared_ptr<ImportStream>>	_stream_map;
	std::deque<std::shared_ptr<ImportMessage>>			_import_message_queue;
	int                                                 _chunk_size;
};




