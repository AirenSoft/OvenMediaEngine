//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "m4s_writer.h"



//====================================================================================================
// Constructor
//====================================================================================================
M4sWriter::M4sWriter(M4sMediaType media_type)
{
	_media_type = media_type;
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
bool M4sWriter::WriteData(const std::shared_ptr<ov::Data> &data, std::shared_ptr<std::vector<uint8_t>> &data_stream)
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
// Text Write
//====================================================================================================
bool M4sWriter::WriteText(std::string value, std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	data_stream->insert(data_stream->end(), value.begin(), value.end());
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
// Data Write(std::vector<uint8_t>) (ov::Data)
//====================================================================================================
bool M4sWriter::WriteData(const std::vector<uint8_t> &data, std::shared_ptr<ov::Data> &data_stream)
{
	data_stream->Append(data.data(), data.size());
	return true;
}

//====================================================================================================
// Data Write(std::shared_ptr<ov::Data>) (ov::Data)
//====================================================================================================
bool M4sWriter::WriteData(const std::shared_ptr<ov::Data> &data, std::shared_ptr<ov::Data> &data_stream)
{
	data_stream->Append(data.get());
	return true;
}

//====================================================================================================
// Data Write (ov::Data)
//====================================================================================================
bool M4sWriter::WriteData(const uint8_t *data, int data_size, std::shared_ptr<ov::Data> &data_stream)
{
	data_stream->Append(data, data_size);
	return true;
}

//====================================================================================================
// write init(ov::Data)
//====================================================================================================
bool M4sWriter::WriteInit(uint8_t value, int init_size, std::shared_ptr<ov::Data> &data_stream)
{
	for (int index = 0; index < init_size; index++)
	{
		WriteUint8(value, data_stream);
	}

	return true;
}

//====================================================================================================
// Text Write (ov::Data)
//====================================================================================================
bool M4sWriter::WriteText(const ov::String &value, std::shared_ptr<ov::Data> &data_stream)
{
	data_stream->Append(value.ToData(false).get());
	return true;
}

//====================================================================================================
// 64bit write (ov::Data)
//====================================================================================================
bool M4sWriter::WriteUint64(uint64_t value, std::shared_ptr<ov::Data> &data_stream)
{
	uint64_t write_value = ov::HostToBE64(value);
	data_stream->Append(&write_value, sizeof(uint64_t));
	return true;
}

//====================================================================================================
// 32bit write (ov::Data)
//====================================================================================================
bool M4sWriter::WriteUint32(uint32_t value, std::shared_ptr<ov::Data> &data_stream)
{
	uint32_t write_value = ov::HostToBE32(value);
	data_stream->Append(&write_value, sizeof(uint32_t));
	return true;
}

//====================================================================================================
// 24bit write(ov::Data)
//====================================================================================================
bool M4sWriter::WriteUint24(uint32_t value, std::shared_ptr<ov::Data> &data_stream)
{
	WriteUint8((uint8_t)(value >> 16 & 0xFF), data_stream);
	WriteUint8((uint8_t)(value >> 8 & 0xFF), data_stream);
	WriteUint8((uint8_t)(value & 0xFF), data_stream);

	return true;
}

//====================================================================================================
// 16bit write (ov::Data)
//====================================================================================================
bool M4sWriter::WriteUint16(uint16_t value, std::shared_ptr<ov::Data> &data_stream)
{
	uint16_t write_value = ov::HostToBE16(value);
	data_stream->Append(&write_value, sizeof(uint16_t));
	return true;
}

//====================================================================================================
// 8bit write (ov::Data)
//====================================================================================================
bool M4sWriter::WriteUint8(uint8_t value, std::shared_ptr<ov::Data> &data_stream)
{
	data_stream->Append(&value, 1);
	return true;
}

//====================================================================================================
// Box Data
// - return : data write size
// - 필요시 64bit size 구현
//====================================================================================================
int M4sWriter::BoxDataWrite(std::string type,
							const std::shared_ptr<std::vector<uint8_t>> &data,
							std::shared_ptr<std::vector<uint8_t>> &data_stream)
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
// Box Data
//====================================================================================================
int M4sWriter::BoxDataWrite(std::string type,
							uint8_t version,
							uint32_t flags,
							const std::shared_ptr<std::vector<uint8_t>> &data,
							std::shared_ptr<std::vector<uint8_t>> &data_stream)
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

//====================================================================================================
// Box Data(ov::Data)
// - return : data write size
// - 필요시 64bit size 구현
//====================================================================================================
int M4sWriter::BoxDataWrite(const ov::String &type,
							const std::shared_ptr<ov::Data> &data,
							std::shared_ptr<ov::Data> &data_stream,
							bool data_size_write/* = false*/)
{
	uint32_t box_size = (data != nullptr) ? MP4_BOX_HEADER_SIZE + data->GetLength() : MP4_BOX_HEADER_SIZE;

	// supported mdat box(data copy decrease)
	if(data_size_write && data != nullptr)
		box_size += sizeof(uint32_t);


	WriteUint32(box_size, data_stream); 	// box size write
	WriteText(type, data_stream);			// type write

	if(data != nullptr)
	{
		if(data_size_write)
			WriteUint32(data->GetLength(), data_stream);

		data_stream->Append(data.get());	// data write
	}

	return data_stream->GetLength();
}

//====================================================================================================
// Box Data(ov::Data)
//====================================================================================================
int M4sWriter::BoxDataWrite(const ov::String &type,
							uint8_t version,
							uint32_t flags,
							const std::shared_ptr<ov::Data> &data,
							std::shared_ptr<ov::Data> &data_stream)
{
	uint32_t box_size = (data != nullptr) ? MP4_BOX_EXT_HEADER_SIZE + data->GetLength() : MP4_BOX_EXT_HEADER_SIZE;

	WriteUint32(box_size, data_stream);	// box size write
	WriteText(type, data_stream);		// type write
	WriteUint8(version, data_stream);	// version write
	WriteUint24(flags, data_stream);    // flag write

	if(data != nullptr)
	{
		data_stream->Append(data.get());// data write
	}

	return data_stream->GetLength();
}
