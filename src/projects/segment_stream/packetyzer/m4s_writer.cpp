//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "m4s_writer.h"

#define MP4_BOX_HEADER_SIZE  (8)        // size(4) + type(4)
#define MP4_BOX_EXT_HEADER_SIZE (12)    // size(4) + type(4) + version(1) + flag(3)

//====================================================================================================
// Constructor
//====================================================================================================
M4sWriter::M4sWriter(M4sMediaType media_type, int data_init_size)
{
	_media_type		= media_type;
	_data_stream	= std::make_shared<std::vector<uint8_t>>();
	_data_stream->reserve(data_init_size);
}

//====================================================================================================
// Text Write 
//====================================================================================================
bool M4sWriter::WriteText(std::string text, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	data_stream->insert(data_stream->end(), text.begin(), text.end());
	return true; 
}

//====================================================================================================
// Data Write(std::vector<uint8_t>)
//====================================================================================================
bool M4sWriter::WriteData(const std::vector<uint8_t> &data, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	data_stream->insert(data_stream->end(), data.begin(), data.end());
	return true;
}

//====================================================================================================
// Data Write(std::shared_ptr<ov::Data>)
//====================================================================================================
bool M4sWriter::WriteData(const std::shared_ptr<ov::Data> &data,  std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
    return WriteData(data->GetDataAs<uint8_t>(), data->GetLength(), data_stream);
}

//====================================================================================================
// Data Write
//====================================================================================================
bool M4sWriter::WriteData(const uint8_t *data, int data_size, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	data_stream->insert(data_stream->end(), data, data + data_size);
	return true;
}

//====================================================================================================
// write init
//====================================================================================================
bool M4sWriter::WriteInit(uint8_t value, int init_size, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	for (int index = 0; index < init_size; index++)
	{
		data_stream->push_back(value);
	}

	return true;
}

//====================================================================================================
// uint32_t write
//====================================================================================================
bool M4sWriter::WriteUint64(uint64_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	data_stream->push_back((uint8_t)(value >> 56 & 0xFF));
	data_stream->push_back((uint8_t)(value >> 48 & 0xFF));
	data_stream->push_back((uint8_t)(value >> 40 & 0xFF));
	data_stream->push_back((uint8_t)(value >> 32 & 0xFF));
	data_stream->push_back((uint8_t)(value >> 24 & 0xFF));
	data_stream->push_back((uint8_t)(value >> 16 & 0xFF));
	data_stream->push_back((uint8_t)(value >> 8 & 0xFF));
	data_stream->push_back((uint8_t)(value & 0xFF));

	return true;
}

//====================================================================================================
// uint32_t write
//====================================================================================================
bool M4sWriter::WriteUint32(uint32_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	data_stream->push_back((uint8_t)(value >> 24 & 0xFF));
	data_stream->push_back((uint8_t)(value >> 16 & 0xFF));
	data_stream->push_back((uint8_t)(value >> 8 & 0xFF));
	data_stream->push_back((uint8_t)(value & 0xFF));

	return true;
}

//====================================================================================================
// 24bit write
//====================================================================================================
bool M4sWriter::WriteUint24(uint32_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	data_stream->push_back((uint8_t)(value >> 16 & 0xFF));
	data_stream->push_back((uint8_t)(value >> 8 & 0xFF));
	data_stream->push_back((uint8_t)(value & 0xFF));
	return true;
}

//====================================================================================================
// 16bit write 
//====================================================================================================
bool M4sWriter::WriteUint16(uint16_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	data_stream->push_back((uint8_t)(value >> 8 & 0xFF));
	data_stream->push_back((uint8_t)(value & 0xFF));
	return true;
}

//====================================================================================================
// 8bit write 
//====================================================================================================
bool M4sWriter::WriteUint8(uint8_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	data_stream->push_back(value);
	return true;
}

//====================================================================================================
// Box 데이터
// - return : data write size
// - 필요시 64bit size 구현
//====================================================================================================
int M4sWriter::BoxDataWrite(std::string type, std::shared_ptr<std::vector<uint8_t>> &data, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	uint32_t box_size = (data != nullptr) ? MP4_BOX_HEADER_SIZE + data->size() : MP4_BOX_HEADER_SIZE;

	WriteUint32(box_size, data_stream); // box size write
	WriteText(type, data_stream);	    // type write

	if(data != nullptr)
	{
		data_stream->insert(data_stream->end(), data->begin(), data->end());// data write
	}

	return data_stream->size();
}

//====================================================================================================
// Box 데이터
//====================================================================================================
int M4sWriter::BoxDataWrite(std::string type, uint8_t version, uint32_t flags, std::shared_ptr<std::vector<uint8_t>> &data, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	uint32_t box_size = (data != nullptr) ? MP4_BOX_EXT_HEADER_SIZE + data->size() : MP4_BOX_EXT_HEADER_SIZE;

	WriteUint32(box_size, data_stream);    // box size write
	WriteText(type, data_stream);          // type write
	WriteUint8(version, data_stream);	   // version write
	WriteUint24(flags, data_stream);       // flag write

	if(data != nullptr)
	{
		data_stream->insert(data_stream->end(), data->begin(), data->end());// data write
	}

	return data_stream->size();
}
