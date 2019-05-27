//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "ts_writer.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <array>

#define TS_STREAM_TYPE_ISO_IEC_13818_7 		(0x0F)
#define TS_STREAM_TYPE_AVC             		(0x1B)
#define PES_HEADER_SIZE						(14)
#define PES_HEADER_WIDTH_DTS_SIZE			(19)
#define TS_DEFAULT_PMT_PID         			(0xFFF)
#define TS_DEFAULT_VIDEO_PID       			(0x100)
#define TS_DEFAULT_AUDIO_PID       			(0x101)
#define TS_DEFAULT_AUDIO_STREAM_ID			(0xc0)
#define TS_DEFAULT_VIDEO_STREAM_ID			(0xe0)
#define TS_PACKET_SIZE         				(188)
#define TS_PACKET_PAYLOAD_SIZE 				(184)
#define TS_HEADER_SIZE						(4)
#define TS_SYNC_BYTE           				(0x47)
#define TS_PCR_ADAPTATION_SIZE 				(6)
#define TS_PAT_SIZE							(13)
#define TS_CRC_SIZE							(4)

static uint8_t const HLS_STUFFING_BYTES[TS_PACKET_SIZE] = 
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF
};

static uint32_t const HLS_CRC_TABLE[256] = 
{
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};
#define H264_AUD_SIZE (6)
static const uint8_t g_aud[H264_AUD_SIZE] = { 0x00 ,0x00 ,0x00 ,0x01 ,0x09 ,0xe0 };

//====================================================================================================
// CRC 생성 
//====================================================================================================
uint32_t TsWriter::MakeCrc(const uint8_t *data, uint32_t data_size)
{
	uint32_t crc = 0xFFFFFFFF;

	for (uint32_t nIndex = 0 ; nIndex < data_size ; nIndex++)
	{
		crc = (crc << 8) ^ HLS_CRC_TABLE[((crc >> 24) ^ *data++) & 0xFF];
	}

	return crc;
}

//====================================================================================================
// Constructor
//====================================================================================================
TsWriter::TsWriter(bool video_enable, bool audio_enable)
{
	_data_stream = std::make_shared<std::vector<uint8_t>>();
	_data_stream->reserve(4096);

	_audio_continuity_count	= 0;
	_video_continuity_count	= 0;

    _video_enable = video_enable;
    _audio_enable = audio_enable;

    WritePAT();
	WritePMT();
}

//====================================================================================================
// Stream Data 버퍼에 데이터 추가
//====================================================================================================
bool TsWriter::WriteDataStream(int data_size, const uint8_t * data)
{
	_data_stream->insert(_data_stream->end(), data, data + data_size);
	return true;
}

//====================================================================================================
// PAT(Program Association Table)  쓰기
// - 프로그램의 번호와 Program Map Table을 담고 있는 패킷의 Packet Identifier(PID) 간의 연결 관계를 담고 있다.
//====================================================================================================
bool TsWriter::WritePAT()
{
	uint32_t payload_size 	= TS_PACKET_PAYLOAD_SIZE;
	uint32_t crc			= 0;

	// TS Header 설정
	MakeTsHeader(0, 0, true, payload_size, false, 0, false);

	//PAT Header 설정(13Byte)
	BitWriter pat_bit(TS_PAT_SIZE);
	pat_bit.Write(8,	0);  					// pointer
	pat_bit.Write(8,	0);  					// table_id
	pat_bit.Write(1,	1);  					// section_syntax_indicator
	pat_bit.Write(1,	0);  					// '0'
	pat_bit.Write(2,	3);  					// reserved
	pat_bit.Write(12,13);					// section_length
	pat_bit.Write(16,1); 					// transport_stream_id
	pat_bit.Write(2,	3);  					// reserved
	pat_bit.Write(5,	0);  					// version_number
	pat_bit.Write(1,	1);  					// current_next_indicator
	pat_bit.Write(8,	0);  					// section_number
	pat_bit.Write(8,	0);  					// last_section_number
	pat_bit.Write(16,1); 					// program number
	pat_bit.Write(3,	7);  					// reserved
	pat_bit.Write(13,TS_DEFAULT_PMT_PID);	// program_map_PID
	WriteDataStream((int)pat_bit.GetDataSize(), pat_bit.GetData());

	crc = MakeCrc(pat_bit.GetData() + 1, (uint32_t)pat_bit.GetDataSize() - 1); // table_id~program_map_PID

	//CRC(4Byte)
	BitWriter crc_bit(TS_CRC_SIZE);
	crc_bit.Write(32, crc);
	WriteDataStream((int)crc_bit.GetDataSize(), crc_bit.GetData());

	//Stuffing Bytes
	WriteDataStream(TS_PACKET_PAYLOAD_SIZE - (TS_PAT_SIZE+TS_CRC_SIZE), HLS_STUFFING_BYTES);

	return true;
}

//====================================================================================================
// PMT(Program Map Table)  쓰기
// -프로그램의 Elementary Stream을 담은 패킷에 대한 연결 정보를 담고 있다.
//====================================================================================================
bool TsWriter::WritePMT()
{
	uint32_t payload_size 	= TS_PACKET_PAYLOAD_SIZE;
	uint32_t section_size 	= 13;
	uint32_t pid			= 0;
	uint32_t crc			= 0;

	// TS Header 설정
	MakeTsHeader(TS_DEFAULT_PMT_PID, 0, true, payload_size, false, 0, false);

	if(_audio_enable)
	{
		section_size += 5;
		pid = TS_DEFAULT_AUDIO_PID;
	}

	if(_video_enable)
	{
		section_size += 5;
		pid = TS_DEFAULT_VIDEO_PID;
	}

	BitWriter pmt_bit(section_size);

	pmt_bit.Write(8,		0);        			// pointer
	pmt_bit.Write(8,		2);        			// table_id
	pmt_bit.Write(1,		1);        			// section_syntax_indicator
	pmt_bit.Write(1,		0);        			// '0'
	pmt_bit.Write(2,		3);        			// reserved
	pmt_bit.Write(12,	section_size); 	// section_length
	pmt_bit.Write(16,	1);       			// program_number
	pmt_bit.Write(2,		3);        			// reserved
	pmt_bit.Write(5,		0);        			// version_number
	pmt_bit.Write(1,		1);        			// current_next_indicator
	pmt_bit.Write(8,		0);        			// section_number
	pmt_bit.Write(8,		0);        			// last_section_number
	pmt_bit.Write(3,		7);        			// reserved
	pmt_bit.Write(13,	pid); 				// PCD_PID
	pmt_bit.Write(4,		0xF);      			// reserved
	pmt_bit.Write(12,	0);      			// program_info_length

	if(_video_enable)
	{
		pmt_bit.Write(8,		TS_STREAM_TYPE_AVC); 			// stream_type
		pmt_bit.Write(3,		0x7);                   		// reserved
		pmt_bit.Write(13,	TS_DEFAULT_VIDEO_PID);    		// elementary_PID
		pmt_bit.Write(4,		0xF);                   		// reserved
		pmt_bit.Write(12,	0);                    			// ES_info_length
	}

	if(_audio_enable)
	{
		pmt_bit.Write(8, 	TS_STREAM_TYPE_ISO_IEC_13818_7);// stream_type
		pmt_bit.Write(3,		0x7);                   		// reserved
		pmt_bit.Write(13,	TS_DEFAULT_AUDIO_PID);    		// elementary_PID
		pmt_bit.Write(4,		0xF);                   		// reserved
		pmt_bit.Write(12,	0);                    			// ES_info_length
	}

	WriteDataStream((int)pmt_bit.GetDataSize(), pmt_bit.GetData());

	crc = MakeCrc(pmt_bit.GetData() + 1, (uint32_t)pmt_bit.GetDataSize() - 1); // table_id~end

	//CRC(4Byte)
	BitWriter crc_bit(TS_CRC_SIZE);
	crc_bit.Write(32, crc);
	WriteDataStream((int)crc_bit.GetDataSize(), crc_bit.GetData());

	//Stuffing Bytes
	WriteDataStream(TS_PACKET_PAYLOAD_SIZE - (section_size + TS_CRC_SIZE), HLS_STUFFING_BYTES);

	return true;
}

//====================================================================================================
// Sample 추가 
// - PES 헤더 추가 (DTS 사용 않함)
//    Header(9Byte) + PTS(5Byte) + [DTS(5Byte)] 
// - TS 헤더 추가 
//====================================================================================================
bool TsWriter::WriteSample(bool is_video,
                            bool is_keyframe,
                            uint64_t timestamp,
                            uint64_t time_offset,
                            std::shared_ptr<ov::Data> &data)
{
	uint8_t pes_header[PES_HEADER_WIDTH_DTS_SIZE] = {0, };
	uint32_t pes_header_size = 0;
	uint32_t rest_data_size = 0;
	uint32_t payload_size = 0;
	const uint8_t *data_pos = nullptr;
	bool first_payload = true;

	//Video(H264) - access unit delimiter(AUD) 정보 추가
	if(is_video)
	{
		data->Insert(g_aud, 0, H264_AUD_SIZE);
	}

	data_pos = data->GetDataAs<uint8_t>();

	// PES Header 생성 
	MakePesHeader(data->GetLength(), is_video, timestamp, time_offset, pes_header, pes_header_size);
	
	// TS Header + Payload 설정 
	rest_data_size = pes_header_size + data->GetLength();
			
	// Buffer 공간 체크 
	// (Payload 개수 + 2)*  TS_PACKET_SIZE
	
	while(rest_data_size > 0)
	{
		payload_size = rest_data_size;

		if(payload_size > TS_PACKET_PAYLOAD_SIZE)
		{
			payload_size = TS_PACKET_PAYLOAD_SIZE;
		}
		
		if(first_payload)
		{
			first_payload = false;
	
			// TS Header 설정 
			if(is_video)
			    MakeTsHeader(TS_DEFAULT_VIDEO_PID,
			            _video_continuity_count++,
			            true,
			            payload_size,
			            true,
			            timestamp*300,
			            is_keyframe);
			else
			    MakeTsHeader(TS_DEFAULT_AUDIO_PID,
			            _audio_continuity_count++,
			            true,
			            payload_size,
			            (!_video_enable && _audio_enable),
			            timestamp*300,
			            is_keyframe);
						
			// PES헤더 설정 
			WriteDataStream(pes_header_size, pes_header); 
			rest_data_size -= pes_header_size;
			payload_size -= pes_header_size; 
		}
		else
		{
			// TS Header 설정 
			if(is_video)
			    MakeTsHeader(TS_DEFAULT_VIDEO_PID, _video_continuity_count++, false, payload_size, false, 0, is_keyframe);
			else
			    MakeTsHeader(TS_DEFAULT_AUDIO_PID, _audio_continuity_count++, false, payload_size, false, 0, is_keyframe);
		}

		// Data 설정
		WriteDataStream(payload_size, data_pos);
		data_pos += payload_size;
		rest_data_size -= payload_size;
	};

	return true; 
}

//====================================================================================================
// PES Header 생성 (Packetized Elementary Stream )
// - 네트워크 전송을 위한 패킷의 크기로 나누어진 Elementary Stream이다. 분할된 첫 조각을 포함하는 패킷에는 PES 헤더라는 정보가 포함되어 전송되며 이후에는 분할된 데이터만 포함된다.
// - PTS : Presentation Time Stamp 
// - DTS : Deciding Time Stamp
// - header 는 최대치인 PES_HEADER_WIDTH_DTS_SIZE로 전달 
//====================================================================================================
bool TsWriter::MakePesHeader(int data_size,
                            bool is_video,
                            uint64_t timestamp,
                            uint64_t time_offset,
                            uint8_t * header,
                            uint32_t & header_size)
{
	uint32_t	stream_id 		= 0;
	uint32_t	pes_packet_size = 0;
	bool		is_dts			= false; 
	uint64_t	pts				= 0; 
	uint64_t	dts				= 0;

	// DTS
	dts = timestamp;

	// PTS
	pts = timestamp + time_offset;
	
	if(is_video)
	{
		stream_id 			= TS_DEFAULT_VIDEO_STREAM_ID; 
		is_dts				= true;
		header_size			= PES_HEADER_WIDTH_DTS_SIZE;
		pes_packet_size 	= 0; //Video 0(초과) 으로 설정 Client 문제시 16bit 한도에서 분활 하여 패킷 정송 고려 
		
	}
	else 
	{
		stream_id 			= TS_DEFAULT_AUDIO_STREAM_ID; 
		is_dts				= false;
		header_size			= PES_HEADER_SIZE;
		pes_packet_size 	= data_size + header_size - 6;
	}

	//PES Header 설정 
	BitWriter pes_bit(header_size); 
	pes_bit.Write(24, 	0x000001);    				// packet_start_code_prefix
	pes_bit.Write(8, 	stream_id);   				// stream_id
	pes_bit.Write(16, 	pes_packet_size); 			// PES_packet_length
	pes_bit.Write(2, 	2);           				// '01'
	pes_bit.Write(2, 	0);            				// PES_scrambling_control
	pes_bit.Write(1, 	0);            				// PES_priority
	pes_bit.Write(1, 	1);            				// data_alignment_indicator
	pes_bit.Write(1, 	0);            				// copyright
	pes_bit.Write(1, 	0);            				// original_or_copy
	pes_bit.Write(2, 	is_dts ? 3:2); 		// PTS_DTS_flags
	pes_bit.Write(1, 	0);            				// ESCR_flag
	pes_bit.Write(1, 	0);           	 			// ES_rate_flag
	pes_bit.Write(1, 	0);            				// DSM_trick_mode_flag
	pes_bit.Write(1, 	0);            				// additional_copy_info_flag
	pes_bit.Write(1, 	0);            				// PES_CRC_flag
	pes_bit.Write(1, 	0);            				// PES_extension_flag
	pes_bit.Write(8, 	header_size - 9);			// PES_header_data_length

	//PTS  
	pes_bit.Write(4, 	is_dts ? 3:2);		// '0010'(PTS) or '0011'(PTS+DTS)
	pes_bit.Write(3, 	(uint32_t)(pts >>30));  		// PTS[32..30]
	pes_bit.Write(1, 	1);                   	 	// marker_bit
	pes_bit.Write(15, 	(uint32_t)(pts >>15)); 		// PTS[29..15]
	pes_bit.Write(1, 	1);                    		// marker_bit
	pes_bit.Write(15, 	(uint32_t)pts);       		// PTS[14..0]
	pes_bit.Write(1, 	1);                    		// market_bit

	//DTS	
	if(is_dts)
	{
		pes_bit.Write(4, 	1);                    	// '0001'
		pes_bit.Write(3, 	(uint32_t)(dts >>30));  	// DTS[32..30]
		pes_bit.Write(1, 	1);                    	// marker_bit
		pes_bit.Write(15, 	(uint32_t)(dts >>15)); 	// DTS[29..15]
		pes_bit.Write(1, 	1);                    	// marker_bit
		pes_bit.Write(15, 	(uint32_t)dts);      	// DTS[14..0]
		pes_bit.Write(1, 	1);                    	// market_bit
	}

	memcpy(header, pes_bit.GetData(), pes_bit.GetDataSize());
	
	return true;
}

//====================================================================================================
// TS Header 설정 
// - PCR : Program Clock Reference
// - Adaptation field control : Playload의 위치가 확인 
// - Continuity counter : 0~15 순환되며  각각 패킷에 부여 
//====================================================================================================
bool TsWriter::MakeTsHeader(int pid,
                            uint32_t continuity_count,
                            bool payload_start,
                            uint32_t & payload_size,
                            bool use_pcr,
                            uint64_t pcr,
                            bool is_keyframe)
{
	uint8_t		header[TS_HEADER_SIZE] 					= {0,};
	uint8_t		adaptation_data[TS_PCR_ADAPTATION_SIZE] = {0,};
	uint32_t	adaptation_field_size 					= 0;
	uint32_t	pcr_size 								= 0;

	header[0] = TS_SYNC_BYTE;
	header[1] = (uint8_t)(((payload_start ? 1 : 0)<<6) | (pid >> 8));
	header[2] = (uint8_t)(pid & 0xFF);

	if(use_pcr)
	{
		adaptation_field_size +=  (2 + TS_PCR_ADAPTATION_SIZE);
	}
	
	// clamp the Payload size
	if(payload_size + adaptation_field_size > TS_PACKET_PAYLOAD_SIZE)
	{
		payload_size = TS_PACKET_PAYLOAD_SIZE - adaptation_field_size;
	}

	// adjust the adaptation field to include stuffing if necessary
	if(adaptation_field_size + payload_size < TS_PACKET_PAYLOAD_SIZE)
	{
		adaptation_field_size = TS_PACKET_PAYLOAD_SIZE - payload_size;
	}

	// no adaptation field
	if(adaptation_field_size == 0)
	{
		header[3] = (uint8_t)(1<<4 | (continuity_count & 0x0F));

		//Segment 데이터 설정 
		WriteDataStream(4, header); 
		
		return true; 
  
	}
	
	// adaptation field present
	header[3] = (uint8_t)(3<<4 | (continuity_count & 0x0F));
	
	//Segment 데이터 설정 
	WriteDataStream(4, header); 

	
	if(adaptation_field_size == 1) 
	{
		adaptation_data[0] = 0; 

		//Segment 데이터 설정 
		WriteDataStream(1, adaptation_data); 

		return true; 		
	} 

	// two or more bytes (stuffing and/or PCR)
	adaptation_data[0] = (uint8_t)(adaptation_field_size - 1);
	adaptation_data[1] = (uint8_t)(use_pcr ? 1<<4 : 0);

	if(use_pcr && payload_start && is_keyframe)
	{	
		adaptation_data[1] =  (uint8_t)(adaptation_data[1] | 1<<6);
	}
	
	//Segment 데이터 설정 
	WriteDataStream(2, adaptation_data); 

	if(use_pcr)
	{
		BitWriter 	pcr_bit(TS_PCR_ADAPTATION_SIZE); 
		uint64_t	pcr_base = pcr/300;
		uint32_t 	pcr_ext  = (uint32_t)(pcr%300);
		
		pcr_bit.Write(1, 	(uint32_t)(pcr_base>>32));
		pcr_bit.Write(32, 	(uint32_t)pcr_base);
		pcr_bit.Write(6, 	0x3F);
		pcr_bit.Write(9, 	pcr_ext);
			
		//Segment 데이터 설정 
		WriteDataStream((int)pcr_bit.GetDataSize(), pcr_bit.GetData());

		//PCR 사이즈 저장 
		pcr_size = TS_PCR_ADAPTATION_SIZE; 
	}

	//Stuffing Bytes 	
	if (adaptation_field_size > 2) 
	{
		
		WriteDataStream(adaptation_field_size - pcr_size - 2, HLS_STUFFING_BYTES); 
	}
   
	return true;
} 
