//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/byte_io.h>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#define MP4_BOX_HEADER_SIZE (8)		  // size(4) + type(4)
#define MP4_BOX_EXT_HEADER_SIZE (12)  // size(4) + type(4) + version(1) + flag(3)

enum class M4sMediaType
{
	Video,
	Audio,
	Data,
};

//====================================================================================================
// Fragment Sample Data
//====================================================================================================
struct SampleData
{
public:
	SampleData(uint64_t duration_,
			   uint32_t flag_,
			   uint64_t timestamp_,
			   uint32_t composition_time_offset_,
			   std::shared_ptr<ov::Data> &data_)
	{
		duration = duration_;
		flag = flag_;
		timestamp = timestamp_;
		composition_time_offset = composition_time_offset_;
		data = data_;
	}

	SampleData(uint64_t duration_,
			   uint64_t timestamp_,
			   std::shared_ptr<ov::Data> &data_)
	{
		duration = duration_;
		flag = 0;
		timestamp = timestamp_;
		composition_time_offset = 0;
		data = data_;
	}

public:
	uint64_t duration;
	uint32_t flag;
	uint64_t timestamp;
	uint32_t composition_time_offset;
	std::shared_ptr<ov::Data> data;
};

//====================================================================================================
// M4sWriter
//====================================================================================================
class M4sWriter
{
public:
	M4sWriter(M4sMediaType media_type);
	virtual ~M4sWriter() = default;

protected:
	// std::vector<uint8_t> process
	bool WriteData(const std::vector<uint8_t> &data, std::shared_ptr<std::vector<uint8_t>> &data_stream);
	bool WriteData(const std::shared_ptr<ov::Data> &data, std::shared_ptr<std::vector<uint8_t>> &data_stream);
	bool WriteData(const uint8_t *data, int data_size, std::shared_ptr<std::vector<uint8_t>> &data_stream);
	bool WriteText(std::string value, std::shared_ptr<std::vector<uint8_t>> &data_stream);
	bool WriteInit(uint8_t value, int init_size, std::shared_ptr<std::vector<uint8_t>> &data_stream);
	bool WriteUint64(uint64_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream);
	bool WriteUint32(uint32_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream);
	bool WriteUint24(uint32_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream);
	bool WriteUint16(uint16_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream);
	bool WriteUint8(uint8_t value, std::shared_ptr<std::vector<uint8_t>> &data_stream);

	// ov::Data process
	bool WriteData(const std::vector<uint8_t> &data, std::shared_ptr<ov::Data> &data_stream);
	bool WriteData(const std::shared_ptr<ov::Data> &data, std::shared_ptr<ov::Data> &data_stream);
	bool WriteData(const uint8_t *data, int data_size, std::shared_ptr<ov::Data> &data_stream);
	bool WriteInit(uint8_t value, int init_size, std::shared_ptr<ov::Data> &data_stream);
	bool WriteText(const ov::String &value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint64(uint64_t value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint24(uint32_t value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint32(uint32_t value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint16(uint16_t value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint8(uint8_t value, std::shared_ptr<ov::Data> &data_stream);

	int BoxDataWrite(std::string type,
					 const std::shared_ptr<std::vector<uint8_t>> &data,
					 std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int BoxDataWrite(std::string type,
					 uint8_t version,
					 uint32_t flags,
					 const std::shared_ptr<std::vector<uint8_t>> &data,
					 std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int BoxDataWrite(const ov::String &type,
					 const std::shared_ptr<ov::Data> &data,
					 std::shared_ptr<ov::Data> &data_stream,
					 bool data_size_write = false);

	int BoxDataWrite(const ov::String &type,
					 uint8_t version,
					 uint32_t flags,
					 const std::shared_ptr<ov::Data> &data,
					 std::shared_ptr<ov::Data> &data_stream);

protected:
	M4sMediaType _media_type;
};