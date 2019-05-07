//==============================================================================
//
//  RtmpProvider
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtmp_mux_util.h" 

uint8_t RtmpMuxUtil::ReadInt8(void * data)
{
	auto *c = (uint8_t *)data;
	uint8_t val;
	val = c[0];
	return val;
}

uint16_t RtmpMuxUtil::ReadInt16(void * data)
{
	auto *c = (uint8_t *)data;
    uint16_t val;
	val = (c[0] << 8) | c[1];
	return val;
}

uint32_t RtmpMuxUtil::ReadInt24(void * data)
{
	auto *c = (uint8_t *)data;
	uint32_t val;
	val = (c[0] << 16) | (c[1] << 8) | c[2];
	return val;
}

uint32_t RtmpMuxUtil::ReadInt32(void * data)
{
	auto *c = (uint8_t *)data;
	uint32_t val;
	val = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
	return val;
}

int RtmpMuxUtil::ReadInt32LE(void * data)
{
	int val;
	memcpy(&val, data, sizeof(int));

	return val;
}

int RtmpMuxUtil::WriteInt8(void * output, uint8_t nVal)
{
	auto *pt_out = (uint8_t*)output;

	pt_out[0] = nVal;
	return sizeof(uint8_t);
}

int RtmpMuxUtil::WriteInt16(void * output, int16_t nVal)
{
	auto *pt_out = (uint8_t*)output;

	pt_out[1] = (uint8_t)(nVal & 0xff);
	pt_out[0] = (uint8_t)(nVal >> 8);
	return sizeof(int16_t);
}

int RtmpMuxUtil::WriteInt24(void * output, int nVal)
{
	auto *pt_out = (uint8_t*)output;

	pt_out[2] = (uint8_t)(nVal & 0xff);
	pt_out[1] = (uint8_t)(nVal >> 8);
	pt_out[0] = (uint8_t)(nVal >> 16);
	return 3;
}

int RtmpMuxUtil::WriteInt32(void * output, int nVal)
{
	auto *pt_out = (uint8_t*)output;
	pt_out[3] = (uint8_t)(nVal & 0xff);
	pt_out[2] = (uint8_t)(nVal >> 8);
	pt_out[1] = (uint8_t)(nVal >> 16);
	pt_out[0] = (uint8_t)(nVal >> 24);
	return sizeof(int);
}

int RtmpMuxUtil::WriteInt32LE(void * output, int nVal)
{
	memcpy(output, &nVal, sizeof(int));
	return sizeof(int);
}


//====================================================================================================
// basic_header 크기 획득(RawData) 
// FormatType + CSID 
//====================================================================================================
int RtmpMuxUtil::GetBasicHeaderSizeByRawData(uint8_t nData)
{
	int header_size = 1; 
	
	switch(nData & RTMP_CHUNK_BASIC_CHUNK_STREAM_ID_MASK)
	{
		case 0:		header_size = 2; 	break;
		case 1:		header_size = 3; 	break;
		default : 	header_size = 1;	break;
	}
	return header_size;
}

//====================================================================================================
// basic_header 크기 획득 (CSID)
// FormatType + CSID 
//====================================================================================================
int RtmpMuxUtil::GetBasicHeaderSizeByChunkStreamID(uint32_t chunk_stream_id)
{
	int	header_size = 1;

	// bh_len 계산
	if		( chunk_stream_id >= (64+256) )	header_size = 3;
	else if( chunk_stream_id >= 64 )		header_size = 2;
	else									header_size = 1;

	return header_size;
}

//====================================================================================================
// 헤더 사이즈 획득 ( basic_header + message_header)
// - Type3는 확장 헤더 확인 못함 - 이전 정보를 기반으로 이후에 확인
//====================================================================================================
int RtmpMuxUtil::GetChunkHeaderSize(void * raw_data, int raw_data_size)
{
	int		    basic_header_size	= 0;
	int		    message_header_size	= 0;
	int		    header_size			= 0;
	auto *	    raw_data_pos 		= (uint8_t*)raw_data;
	uint8_t	    format_type;
	uint32_t	chunk_stream_id;

	// 파라미터 체크
	if(raw_data == nullptr || raw_data_size == 0) 
	{ 
		return 0; 
	}

	// FormatType 및 ChunkStreamID 구하기
	format_type		= (uint8_t)(raw_data_pos[0] & RTMP_CHUNK_BASIC_FORMAT_TYPE_MASK);
	chunk_stream_id	= (uint32_t)(raw_data_pos[0] & RTMP_CHUNK_BASIC_CHUNK_STREAM_ID_MASK);

	// basic_header  길이 
	if			( chunk_stream_id == 0 )	basic_header_size = 2;
	else if	( chunk_stream_id == 1 )	basic_header_size = 3;
	else 									basic_header_size = 1;
		
	// msg header의 길이 구하기
	switch(format_type)
	{
		case RTMP_CHUNK_BASIC_FORMAT_TYPE0:	message_header_size = RTMP_CHUNK_BASIC_FORMAT_TYPE0_SIZE;	break;
		case RTMP_CHUNK_BASIC_FORMAT_TYPE1:	message_header_size = RTMP_CHUNK_BASIC_FORMAT_TYPE1_SIZE;	break;
		case RTMP_CHUNK_BASIC_FORMAT_TYPE2:	message_header_size = RTMP_CHUNK_BASIC_FORMAT_TYPE2_SIZE;	break;
		case RTMP_CHUNK_BASIC_FORMAT_TYPE3:	message_header_size = RTMP_CHUNK_BASIC_FORMAT_TYPE3_SIZE;	break;
		default : 							return -1;
	}

	header_size = basic_header_size + message_header_size; 

	// 데이터 수신 확인 
	if(raw_data_size < header_size)
	{
		return 0; 
	}	

	//Extended Timestamp 확인(Type3는 확인 못함 - 이전 정보를 기반으로 이후에 확인) 
	if( format_type == RTMP_CHUNK_BASIC_FORMAT_TYPE0 || format_type == RTMP_CHUNK_BASIC_FORMAT_TYPE1 || format_type == RTMP_CHUNK_BASIC_FORMAT_TYPE2)
	{
		if( ReadInt24(raw_data_pos + basic_header_size) == RTMP_EXTEND_TIMESTAMP )
		{ 
			header_size += RTMP_EXTEND_TIMESTAMP_SIZE; 

			// 데이터 수신 확인 
			if(raw_data_size < header_size)
			{
				return 0; 
			}
			
		}
	}

	return  header_size;
}


//====================================================================================================
//Chunk 헤더 획득 
// - Type3는 확장 헤더 확인 못함 - 이전 정보를 기반으로 이후에 확인
//====================================================================================================
std::shared_ptr<RtmpChunkHeader> RtmpMuxUtil::GetChunkHeader(void * raw_data, int raw_data_size,  int &chunk_header_size, bool &extend_type)
{
	int			basic_header_size	= 0;
	auto *		raw_data_pos		= (uint8_t*)raw_data;
	uint8_t	    format_type			= 0;
	uint32_t	chunk_stream_id		= 0;

	chunk_header_size   = 0;
    extend_type         = false;

	//Param 확인
	if( raw_data == nullptr || raw_data_size == 0)
	{
		return nullptr;
	}

	// 헤더길이 얻기
	basic_header_size	= GetBasicHeaderSizeByRawData(raw_data_pos[0]);
    chunk_header_size	= GetChunkHeaderSize(raw_data, raw_data_size);

	if(chunk_header_size <= 0)
	{
		return nullptr;
	}

	auto chunk_header = std::make_shared<RtmpChunkHeader>();

	//Format Type 구하기
	format_type = (uint8_t)(raw_data_pos[0] & RTMP_CHUNK_BASIC_FORMAT_TYPE_MASK);
	
	//Chunk Stream ID 구하기
	chunk_stream_id = (uint32_t)(raw_data_pos[0] & RTMP_CHUNK_BASIC_CHUNK_STREAM_ID_MASK);
	
	if		( chunk_stream_id == 0 ) 	chunk_stream_id = (uint32_t)(64 + raw_data_pos[1]);
	else if( chunk_stream_id == 1 )	chunk_stream_id = (uint32_t)(64 + raw_data_pos[1] + raw_data_pos[2]*256);

	chunk_header->basic_header.format_type 		= format_type;
	chunk_header->basic_header.chunk_stream_id 	= chunk_stream_id;

	raw_data_pos += basic_header_size; 
	
	//Message Header 구하기
	switch(format_type)
	{
		case RTMP_CHUNK_BASIC_FORMAT_TYPE0:
		{
			chunk_header->type_0.timestamp		= ReadInt24(raw_data_pos);
			raw_data_pos += 3; 
			
			chunk_header->type_0.body_size			= ReadInt24(raw_data_pos);
			raw_data_pos += 3; 
			
			chunk_header->type_0.type_id			= raw_data_pos[0];
			raw_data_pos += 1; 
			
			chunk_header->type_0.stream_id		= (uint32_t)ReadInt32LE(raw_data_pos);
			raw_data_pos += 4; 

			//Externed Timestamp 확인 
			if(chunk_header->type_0.timestamp == RTMP_EXTEND_TIMESTAMP)
			{
				chunk_header->type_0.timestamp	= ReadInt32(raw_data_pos);
                extend_type = true;
			}
			break;
		}
		case RTMP_CHUNK_BASIC_FORMAT_TYPE1:
		{
			chunk_header->type_1.timestamp_delta	= ReadInt24(raw_data_pos);
			raw_data_pos += 3; 
			
			chunk_header->type_1.body_size			= ReadInt24(raw_data_pos);
			raw_data_pos += 3; 
			
			chunk_header->type_1.type_id			= raw_data_pos[0];
			raw_data_pos += 1; 

			// Externed Timestamp 확인
			if(chunk_header->type_1.timestamp_delta == RTMP_EXTEND_TIMESTAMP)
			{
				chunk_header->type_1.timestamp_delta = ReadInt32(raw_data_pos);
                extend_type = true;
			}
				
			break;
		}
		case RTMP_CHUNK_BASIC_FORMAT_TYPE2:
		{
			chunk_header->type_2.timestamp_delta	= ReadInt24(raw_data_pos);
			raw_data_pos += 3; 

			// Externed Timestamp 확인
			if(chunk_header->type_1.timestamp_delta == RTMP_EXTEND_TIMESTAMP)
			{
				chunk_header->type_1.timestamp_delta = ReadInt32(raw_data_pos);
                extend_type = true;
			}
			
			break;
		}
		default : break;
	}

	return chunk_header;
}

//====================================================================================================
// RawData 에서 ChunkData 획득 
// - RawData :     [Block] + [Type3 Header] + [Block] + [Type3 Header] + [Block] ..... 구조 
//====================================================================================================
int RtmpMuxUtil::GetChunkData(int chunk_size, void * raw_data, int raw_data_size, int chunk_data_size, void * chunk_data, bool extend_type)
{
	auto *	raw_data_pos		= (uint8_t*)raw_data;
	auto * chunk_data_pos		= (uint8_t*)chunk_data; 
	int		block_count			= 0;
	int		need_data_size		= 0;
	int		read_data_size		= 0;
	int		type3_header_size 	= 1;
		
	if(extend_type)
	{
		type3_header_size += RTMP_EXTEND_TIMESTAMP_SIZE; 
	}

	// 파라미터 체크
	if(	chunk_size == 0 || raw_data == nullptr || raw_data_size == 0 || chunk_data_size == 0 || chunk_data == nullptr )
	{
		return 0; 
	}

	// 초기화
	read_data_size 	= 0;
	block_count 	= ((chunk_data_size-1)/chunk_size);
	block_count++; 
	need_data_size 	= chunk_data_size + (block_count-1)*type3_header_size;

	// 필요한 길이만큼 수신이 안되었다면 스킵
	if(raw_data_size < need_data_size)
	{ 
		return 0; 
	}

	// 수신
	for(int nIndex=0 ; nIndex < block_count; nIndex++)
	{
		int size = 0; 
		
		// 블럭 구별용 type3 헤더 처리
		if( nIndex > 0 )
		{
			// type3인지 체크. 아니면 에러리턴
			if(GetBasicHeaderSizeByRawData(*raw_data_pos) != 1)
			{ 
				return 0; 
			}

			//Type3 헤더 Offset 이동 
			read_data_size	+= type3_header_size;
			raw_data_pos		+= type3_header_size; 
		}

		// 데이터 복사  크기 설정 
		size = (need_data_size - read_data_size) > chunk_size ? chunk_size : (need_data_size - read_data_size);
			
		// 데이터 복사 
		memcpy(chunk_data_pos, raw_data_pos, (size_t)size);

		// 읽은크기 재설정
		chunk_data_pos 	+= size;
		raw_data_pos	+= size; 	
		read_data_size 	+= size;
	}

	return read_data_size;
}

//====================================================================================================
// Basic 헤더 생성 
//====================================================================================================
int RtmpMuxUtil::GetChunkBasicHeaderRaw(uint8_t chunk_type, uint32_t chunk_stream_id, void * raw_data)
{

	auto * raw_data_pos 		= (uint8_t*)raw_data;
	int		basic_header_size	= 0;

	// 파라미터 체크
	if(raw_data == nullptr) 
	{ 
		return 0; 
	}

	// basic_header 만들기
	if(chunk_stream_id >= (64+256))
	{
		basic_header_size 	= 3;
		raw_data_pos[0] 	= 1;			
		chunk_stream_id  	-= 64;
		raw_data_pos[1] 	= (uint8_t)(chunk_stream_id&0xff);
		chunk_stream_id 	>>= 8;
		raw_data_pos[2] 	= (uint8_t)(chunk_stream_id&0xff);
	}
	else if(chunk_stream_id >= 64)
	{
		basic_header_size 	= 2;
		raw_data_pos[0] 	= 0;		
		chunk_stream_id  	-= 64;
		raw_data_pos[1] 	= (uint8_t)(chunk_stream_id&0xff);
	}
	else
	{
		basic_header_size 	= 1;
		raw_data_pos[0] 	= (uint8_t)(chunk_stream_id&0x3f);
	}

	// FormatType 기록
	raw_data_pos[0] |= chunk_type & 0xc0;

	return basic_header_size;
}

//====================================================================================================
// Chunk 헤더 생성 
// basic_header + message_header
//====================================================================================================
int RtmpMuxUtil::GetChunkHeaderRaw(std::shared_ptr<RtmpChunkHeader> &chunk_header, void *raw_data, bool extend_type)
{
	auto 	*raw_data_pos 		= (uint8_t*)raw_data;
	int		basic_header_size	= 0;
	int 	message_header_size	= 0;

	// 파라미터 체크
	if( chunk_header == nullptr || raw_data == nullptr )
	{ 
		return 0; 
	}

	// basic_header 만들기
	basic_header_size = GetChunkBasicHeaderRaw(chunk_header->basic_header.format_type, chunk_header->basic_header.chunk_stream_id, raw_data);

	raw_data_pos += basic_header_size; 
	
	// msg header 만들기
	switch(chunk_header->basic_header.format_type)
	{
		case RTMP_CHUNK_BASIC_FORMAT_TYPE0:
		{
			message_header_size = RTMP_CHUNK_BASIC_FORMAT_TYPE0_SIZE;
			
			//timestamp 
			if(!extend_type)	WriteInt24(raw_data_pos, chunk_header->type_0.timestamp);
			else				WriteInt24(raw_data_pos, RTMP_EXTEND_TIMESTAMP);
			raw_data_pos += 3; 

			//length 
			WriteInt24(raw_data_pos, chunk_header->type_0.body_size);
			raw_data_pos += 3; 
			
			//typeid
			*(raw_data_pos) = chunk_header->type_0.type_id;
			raw_data_pos += 1; 
			
			//streamid
			WriteInt32LE(raw_data_pos, chunk_header->type_0.stream_id);
			raw_data_pos += 4; 

			//extended timestamp
			if(extend_type)
			{
				message_header_size += RTMP_EXTEND_TIMESTAMP_SIZE; 
				WriteInt32(raw_data_pos, chunk_header->type_0.timestamp);
			}
			
			break;
		}
		case RTMP_CHUNK_BASIC_FORMAT_TYPE1:
		{
			message_header_size = RTMP_CHUNK_BASIC_FORMAT_TYPE1_SIZE;
			
			//timestamp 
			if(!extend_type)	WriteInt24(raw_data_pos, chunk_header->type_1.timestamp_delta);
			else				WriteInt24(raw_data_pos, RTMP_EXTEND_TIMESTAMP);
			raw_data_pos += 3; 

			// length
			WriteInt24(raw_data_pos, chunk_header->type_1.body_size);
			raw_data_pos += 3; 
			
			// streamid
			*(raw_data_pos) = chunk_header->type_1.type_id;
			raw_data_pos += 1; 

			// extended timestamp
			if(extend_type)
			{
				message_header_size += RTMP_EXTEND_TIMESTAMP_SIZE; 
				WriteInt32(raw_data_pos, chunk_header->type_1.timestamp_delta);
			}
			
			break;
		}
		case RTMP_CHUNK_BASIC_FORMAT_TYPE2:
		{
			message_header_size = RTMP_CHUNK_BASIC_FORMAT_TYPE2_SIZE;
			
			//timestamp
			if(!extend_type)
			{
				
				WriteInt24(raw_data_pos, chunk_header->type_2.timestamp_delta);
			}
			else
			{
				message_header_size += RTMP_EXTEND_TIMESTAMP_SIZE;
				WriteInt24(raw_data_pos, RTMP_EXTEND_TIMESTAMP);										
				raw_data_pos += 3; 
				
				WriteInt32(raw_data_pos, chunk_header->type_2.timestamp_delta);
				
			}
			
			break;
		}
		default : break;
	}

	return (basic_header_size + message_header_size);
}

//====================================================================================================
// 최소 청크 구성 사이즈 계산
// chunk header + block + chunk header(type3) + block + chunk header(type3) + block....
//====================================================================================================
int RtmpMuxUtil::GetChunkDataRawSize(int chunk_size, uint32_t chunk_stream_id, int chunk_data_size, bool extend_type)
{
	int type3_header_size 	= 0;
	int block_count			= 0; 
	
	// 파라미터 체크
	if(chunk_size == 0 || chunk_data_size == 0 )
	{ 
		return 0; 
	}
	
	// BashHeader 구하기
	type3_header_size = GetBasicHeaderSizeByChunkStreamID(chunk_stream_id);

	if(extend_type)
	{
		type3_header_size += RTMP_EXTEND_TIMESTAMP_SIZE; //Extended Timestamp Header 정보가 출력 
	}
	
	block_count = (chunk_data_size-1) / chunk_size; 
	
	// 계산
	return chunk_data_size + block_count*type3_header_size;
}

//====================================================================================================
// ChunkData 에서 RawData 획득 
// - RawData :     [Block] + [Type3 Header] + [Block] + [Type3 Header] + [Block] ..... 구조 
//====================================================================================================
int RtmpMuxUtil::GetChunkDataRaw(int chunk_size, uint32_t chunk_stream_id, std::shared_ptr<std::vector<uint8_t>> &chunk_data, void * raw_data, bool extend_type, uint32_t time)
{
	auto    *raw_data_pos		= (uint8_t*)raw_data;
	uint8_t	type3_header[RTMP_CHUNK_BASIC_HEADER_SIZE_MAX] = {0,};
	int		type3_header_size 	= 0;
	int		read_data_size		= 0;
	int		block_count 		= 0; 
	int		raw_data_size		= 0; 
	
	// 파라미터 체크
	if(chunk_size == 0 || chunk_data->empty()  || raw_data == nullptr )
	{ 
		return 0; 
	}

	// 초기화
	block_count		= (chunk_data->size() - 1)/chunk_size;
	block_count++; 
	
	// basic_header 만들기
	type3_header_size = GetChunkBasicHeaderRaw(RTMP_CHUNK_BASIC_FORMAT_TYPE3, chunk_stream_id, type3_header);

	// 작성
	for(int nIndex = 0 ; nIndex  < block_count ; nIndex++)
	{
		int size = 0;
		
		// type3 헤더 기록
		if( nIndex > 0 )
		{
			memcpy(raw_data_pos, type3_header, (size_t)type3_header_size);
			raw_data_pos 	+= type3_header_size;
			raw_data_size 	+= type3_header_size;
				
			//Timestamp Extended Header 기록 
			if(extend_type)
			{
				WriteInt32(raw_data_pos, time);
				raw_data_pos		+= RTMP_EXTEND_TIMESTAMP_SIZE;
				raw_data_size	+= RTMP_EXTEND_TIMESTAMP_SIZE;
			}
		}

		// 읽어들일 크기 설정
        size = (static_cast<int>(chunk_data->size()) - read_data_size) > chunk_size ?
                chunk_size :
	            (static_cast<int>(chunk_data->size()) - read_data_size);

		// 읽어들이기
		memcpy(raw_data_pos, chunk_data->data() + read_data_size, (size_t)size);

		// 읽은크기 재설정
		read_data_size	+= size;
		raw_data_pos	+= size;
		raw_data_size	+= size;
	}

	return raw_data_size;
}

