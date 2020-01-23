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
#include <memory>
#include <string>
#include <vector>

#include <base/info/application.h>
#include <base/ovlibrary/byte_io.h>

#define MP4_BOX_HEADER_SIZE (8)		  // size(4) + type(4)
#define MP4_BOX_EXT_HEADER_SIZE (12)  // size(4) + type(4) + version(1) + flag(3)

enum class M4sMediaType
{
	Video,
	Audio,
	Data,
};

// Data for Fragment MP4
struct SampleData
{
public:
	SampleData(uint64_t duration,
			   uint32_t flag,
			   int64_t timestamp,
			   uint32_t composition_time_offset,
			   std::shared_ptr<ov::Data> &data)
		: duration(duration),
		  flag(flag),
		  timestamp(timestamp),
		  composition_time_offset(composition_time_offset),
		  data(data)
	{
	}

	SampleData(uint64_t duration,
			   int64_t timestamp,
			   std::shared_ptr<ov::Data> &data)
		: duration(duration),
		  timestamp(timestamp),
		  data(data)
	{
	}

	uint64_t duration = 0ULL;
	uint32_t flag = 0U;
	int64_t timestamp = 0LL;
	uint32_t composition_time_offset = 0U;
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
	// ov::Data process
	bool WriteData(const std::vector<uint8_t> &data, std::shared_ptr<ov::Data> &data_stream);
	bool WriteData(const std::shared_ptr<const ov::Data> &data, std::shared_ptr<ov::Data> &data_stream);
	bool WriteData(const uint8_t *data, int data_size, std::shared_ptr<ov::Data> &data_stream);
	bool WriteInit(uint8_t value, int init_size, std::shared_ptr<ov::Data> &data_stream);
	bool WriteText(const ov::String &value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint64(uint64_t value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint24(uint32_t value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint32(uint32_t value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint16(uint16_t value, std::shared_ptr<ov::Data> &data_stream);
	bool WriteUint8(uint8_t value, std::shared_ptr<ov::Data> &data_stream);

	int WriteBoxData(const ov::String &type,
					 const std::shared_ptr<ov::Data> &data,
					 std::shared_ptr<ov::Data> &data_stream,
					 bool data_size_write = false);

	int WriteBoxData(const ov::String &type,
					 uint8_t version,
					 uint32_t flags,
					 const std::shared_ptr<ov::Data> &data,
					 std::shared_ptr<ov::Data> &data_stream);

protected:
	M4sMediaType _media_type;
};