//==============================================================================
//
//  RtmpProvider
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "rtmp_import_chunk.h"
#include "../rtmp_chunk_log_define.h"

//====================================================================================================
// RtmpImportChunk
//====================================================================================================
RtmpImportChunk::RtmpImportChunk(int chunk_size) {
    _stream_map.clear();
    _import_message_queue.clear();
    _chunk_size = chunk_size;
}

//====================================================================================================
// ~RtmpImportChunk
//====================================================================================================
RtmpImportChunk::~RtmpImportChunk()
{
    Destroy();
}

//====================================================================================================
// 종료
//====================================================================================================
void RtmpImportChunk::Destroy()
{
    _stream_map.clear();
    _import_message_queue.clear();
}

//====================================================================================================
// 스트림 획득
//====================================================================================================
std::shared_ptr<ImportStream> RtmpImportChunk::GetStream(uint32_t chunk_stream_id)
{
    auto item = _stream_map.find(chunk_stream_id);

    //이미 존재
    if (item != _stream_map.end())
    {
        return item->second;
    }

    //메모리 할당
    auto stream = std::make_shared<ImportStream>();

    // 등록
    _stream_map.insert(std::pair<uint32_t, std::shared_ptr<ImportStream>>(chunk_stream_id, stream));

    return stream;
}

//====================================================================================================
// 메시지 헤더 정보 설정
//====================================================================================================
std::shared_ptr<RtmpMuxMessageHeader> RtmpImportChunk::GetMessageHeader(std::shared_ptr<ImportStream> &stream,
                                                                        std::shared_ptr<RtmpChunkHeader> &chunk_header)
{
    auto message_header = std::make_shared<RtmpMuxMessageHeader>();

    switch (chunk_header->basic_header.format_type)
    {
        case RTMP_CHUNK_BASIC_FORMAT_TYPE0:
        {
            message_header->chunk_stream_id = chunk_header->basic_header.chunk_stream_id;
            message_header->timestamp = chunk_header->type_0.timestamp;
            message_header->body_size = chunk_header->type_0.body_size;
            message_header->type_id = chunk_header->type_0.type_id;
            message_header->stream_id = chunk_header->type_0.stream_id;
            break;
        }
        case RTMP_CHUNK_BASIC_FORMAT_TYPE1:
        {
            message_header->chunk_stream_id = chunk_header->basic_header.chunk_stream_id;
            message_header->timestamp = stream->message_header->timestamp + chunk_header->type_1.timestamp_delta;
            message_header->body_size = chunk_header->type_1.body_size;
            message_header->type_id = chunk_header->type_1.type_id;
            message_header->stream_id = stream->message_header->stream_id;        //
            break;
        }
        case RTMP_CHUNK_BASIC_FORMAT_TYPE2:
        {
            message_header->chunk_stream_id = chunk_header->basic_header.chunk_stream_id;
            message_header->timestamp = stream->message_header->timestamp + chunk_header->type_2.timestamp_delta;
            message_header->body_size = stream->message_header->body_size;
            message_header->type_id = stream->message_header->type_id;
            message_header->stream_id = stream->message_header->stream_id;
            break;
        }
        case RTMP_CHUNK_BASIC_FORMAT_TYPE3:
        {
            message_header->chunk_stream_id = chunk_header->basic_header.chunk_stream_id;
            message_header->timestamp = stream->message_header->timestamp + stream->timestamp_delta;
            message_header->body_size = stream->message_header->body_size;
            message_header->type_id = stream->message_header->type_id;
            message_header->stream_id = stream->message_header->stream_id;
            break;
        }
        default :
            break;
    }

    return message_header;
}

//====================================================================================================
// ChunkMessage 추가
//====================================================================================================
bool
RtmpImportChunk::AppendChunk(std::shared_ptr<ImportStream> &stream, uint8_t *chunk, int header_size, int data_size)
{
    int append_size = header_size + data_size;

    //10M이상 데이터 경고
    if (append_size > RTMP_MAX_PACKET_SIZE)//10M
    {
        RTMP_CHUNK_WARNING_LOG(("Rtmp Append Data Size ( %d)", append_size));
        return false;
    }

    // 버퍼에 남아있는공간 체크. 부족하면 새로 할당
    if ((static_cast<int>(stream->buffer->size()) - stream->data_size) < append_size)
    {
        stream->buffer->resize(
                (stream->buffer->size() * 2) > (stream->buffer->size() + append_size) ? (stream->buffer->size() * 2) : (
                        stream->buffer->size() + append_size));
    }

    // 복사
    memcpy(stream->buffer->data() + stream->data_size, chunk, (size_t) append_size);
    stream->data_size += append_size;
    stream->write_chunk_size += data_size;

    return true;
}

//====================================================================================================
// ChunkData 수신 완료 데이터 복사
//====================================================================================================
int RtmpImportChunk::CompleteChunkMessage(std::shared_ptr<ImportStream> &stream, int chunk_size)
{
    int chunk_header_size = 0;
    int chunk_data_raw_size = 0;
    bool extend_type = false;

    //Chunk Header 읽기
    auto chunk_header = GetChunkHeader(stream->buffer->data(), stream->data_size, chunk_header_size, extend_type);

    //Message Header 설정
    auto message_header = GetMessageHeader(stream, chunk_header);

    //전체 크기 확인
    chunk_data_raw_size = GetChunkDataRawSize(chunk_size, message_header->chunk_stream_id, message_header->body_size,
                                              extend_type);

    // Chunk Stream 이 완성되지 않았다면 스킵
    if (stream->data_size < chunk_header_size + chunk_data_raw_size)
    {
        RTMP_CHUNK_ERROR_LOG(("Buffer(%d) RawData(%d)", stream->data_size, chunk_header_size + chunk_data_raw_size));
        return 0;
    }

    // 데이터 길이가 0인 헤더 정보 정보가 오는 경우 발생
    if (message_header->body_size == 0)
    {
        //스트림 정보 초기화
        stream->data_size = 0;
        stream->write_chunk_size = 0;
        return 0;
    }

    // 메모리 할당
    auto message = std::make_shared<ImportMessage>(message_header);

    //Chunk Message 할당
    if (GetChunkData(chunk_size, stream->buffer->data() + chunk_header_size, chunk_data_raw_size,
                     message_header->body_size, message->body->data(), extend_type) == 0)
    {
        RTMP_CHUNK_ERROR_LOG(("GetChunkData - Header(%d) RawData(%d)", chunk_header_size, chunk_data_raw_size));
        return 0;
    }

    // 스트림 정보 초기화
    stream->data_size = 0;
    stream->write_chunk_size = 0;

    // 리스트에 등록
    _import_message_queue.push_back(message);

    return message_header->body_size;
}

//====================================================================================================
// ImportStream
// - Chunk 데이터 삽입
//====================================================================================================
int RtmpImportChunk::ImportStreamData(uint8_t *data, int data_size, bool &message_complete)
{
    int chunk_header_size = 0;
    int rest_chunk_size = 0;
    bool extend_type = false;

    if (data_size <= 0 || data == nullptr)
    {
        return 0;
    }

    // Chunk Header  읽기
    auto chunk_header = GetChunkHeader(data, data_size, chunk_header_size, extend_type);

    if (chunk_header_size <= 0)
    {
        return chunk_header_size;
    }

    // 스트림 찾기
    auto stream = GetStream(chunk_header->basic_header.chunk_stream_id);

    // Type3 ExtendHeader 확인
    if (chunk_header->basic_header.format_type == RTMP_CHUNK_BASIC_FORMAT_TYPE3)
    {
        extend_type = stream->extend_type;

        if (extend_type)
        {
            chunk_header_size += RTMP_EXTEND_TIMESTAMP_SIZE;

            if (data_size < chunk_header_size)
            {
                RTMP_CHUNK_INFO_LOG(("Type3 Header Extend Size Cut"));
                return 0;
            }
        }
    }

    // ExtendHeader 설정
    stream->extend_type = extend_type;

    // Message Header 얻기
    auto message_header = GetMessageHeader(stream, chunk_header);

    rest_chunk_size = message_header->body_size - stream->write_chunk_size;

    if (rest_chunk_size <= 0)
    {
        RTMP_CHUNK_ERROR_LOG(
                ("Rest Chunk Size Fail - Header(%d) Data(%d) Chunk(%d)", chunk_header_size, rest_chunk_size, _chunk_size));
        return -1;
    }

    // Chunk Size 가 남은 경우
    if (_chunk_size < rest_chunk_size)
    {
        //Data 를 아직 다 받지 못했다면 스킵
        if (data_size < chunk_header_size + _chunk_size)
        {
            return 0;
        }

        // Chunk 추가
        if (!AppendChunk(stream, data, chunk_header_size, _chunk_size))
        {
            RTMP_CHUNK_ERROR_LOG(
                    ("AppendChunk Fail - Header(%d) Data(%d) Chunk(%d)", chunk_header_size, rest_chunk_size, _chunk_size));
            return -1;
        }

        // Import 스트림 정보 업데이트
        message_header->timestamp -= stream->timestamp_delta;
        stream->message_header = message_header;
        message_complete = false;

        return chunk_header_size + _chunk_size;
    }

    // Data 를 아직 다 받지 못했다면 skip
    if (data_size < chunk_header_size + rest_chunk_size)
    {
        return 0;
    }

    // Chunk 추가
    if (!AppendChunk(stream, data, chunk_header_size, rest_chunk_size))
    {
        RTMP_CHUNK_ERROR_LOG(
                ("AppendChunk Fail - Header(%d) Data(%d) Chunk(%d)", chunk_header_size, rest_chunk_size, _chunk_size));
        return -1;
    }

    // 완료
    CompleteChunkMessage(stream, _chunk_size);

    // header 정보 갱신
    stream->timestamp_delta = message_header->timestamp - stream->message_header->timestamp;
    stream->message_header = message_header;
    message_complete = true;

    return chunk_header_size + rest_chunk_size;

}

//====================================================================================================
// Import Message
//====================================================================================================
std::shared_ptr<ImportMessage> RtmpImportChunk::GetMessage()
{
    // 리스트에 아무것도 없다면 에러리턴
    if (_import_message_queue.empty())
    {
        return nullptr;
    }

    auto message = _import_message_queue.front();

    _import_message_queue.pop_front();

    return message;
}
