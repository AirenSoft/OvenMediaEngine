//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "m4s_segment_writer.h"

#define DEFAULT_SEGMENT_HEADER_SIZE (4096)

M4sSegmentWriter::M4sSegmentWriter(M4sMediaType media_type, uint32_t sequence_number, uint32_t track_id, int64_t start_timestamp)
	: M4sWriter(media_type),

	  _sequence_number(sequence_number),
	  _track_id(track_id),
	  _start_timestamp(start_timestamp)
{
}

M4sSegmentWriter::~M4sSegmentWriter()
{
}

const std::shared_ptr<ov::Data> M4sSegmentWriter::AppendSamples(const std::vector<std::shared_ptr<const SampleData>> &sample_datas)
{
	uint32_t total_sample_size = 0;

	for (const auto &sample_data : sample_datas)
	{
		total_sample_size += sample_data->data->GetLength();

		if (_media_type == M4sMediaType::Video)
		{
			total_sample_size += sizeof(uint32_t);
		}
	}

	auto data_stream = std::make_shared<ov::Data>(total_sample_size + DEFAULT_SEGMENT_HEADER_SIZE);

	WriteMoofBox(data_stream, sample_datas);
	WriteMdatBox(data_stream, sample_datas, total_sample_size);

	return data_stream;
}

int M4sSegmentWriter::WriteMoofBox(std::shared_ptr<ov::Data> &data_stream, const std::vector<std::shared_ptr<const SampleData>> &sample_datas)
{
	auto data = std::make_shared<ov::Data>(DEFAULT_SEGMENT_HEADER_SIZE);

	WriteMfhdBox(data);
	WriteTrafBox(data, sample_datas);

	WriteBoxData("moof", data, data_stream);

	int position = (_media_type == M4sMediaType::Video) ? data_stream->GetLength() - sample_datas.size() * 16 - 4 : data_stream->GetLength() - sample_datas.size() * 8 - 4;

	if (position < 0)
	{
		return -1;
	}

	// trun data offset value change
	ByteWriter<uint32_t>::WriteBigEndian(data_stream->GetWritableDataAs<uint8_t>() + position, data_stream->GetLength() + 8);

	return data_stream->GetLength();
}

int M4sSegmentWriter::WriteMfhdBox(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(_sequence_number, data);  // Sequence Number

	return WriteBoxData("mfhd", 0, 0, data, data_stream);
}

int M4sSegmentWriter::WriteTrafBox(std::shared_ptr<ov::Data> &data_stream, const std::vector<std::shared_ptr<const SampleData>> &sample_datas)
{
	auto data = std::make_shared<ov::Data>();

	WriteTfhdBox(data);
	WriteTfdtBox(data);
	WriteTrunBox(data, sample_datas);

	return WriteBoxData("traf", data, data_stream);
}

#define TFHD_FLAG_BASE_DATA_OFFSET_PRESENT (0x00001)
#define TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT (0x00002)
#define TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT (0x00008)
#define TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT (0x00010)
#define TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT (0x00020)
#define TFHD_FLAG_DURATION_IS_EMPTY (0x10000)
#define TFHD_FLAG_DEFAULT_BASE_IS_MOOF (0x20000)

int M4sSegmentWriter::WriteTfhdBox(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();
	uint32_t flag = TFHD_FLAG_DEFAULT_BASE_IS_MOOF;

	WriteUint32(_track_id, data);  // track id

	return WriteBoxData("tfhd", 0, flag, data, data_stream);
}

int M4sSegmentWriter::WriteTfdtBox(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint64(_start_timestamp, data);  // Base media decode time

	return WriteBoxData("tfdt", 1, 0, data, data_stream);
}

#define TRUN_FLAG_DATA_OFFSET_PRESENT (0x0001)
#define TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT (0x0004)
#define TRUN_FLAG_SAMPLE_DURATION_PRESENT (0x0100)
#define TRUN_FLAG_SAMPLE_SIZE_PRESENT (0x0200)
#define TRUN_FLAG_SAMPLE_FLAGS_PRESENT (0x0400)
#define TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT (0x0800)

int M4sSegmentWriter::WriteTrunBox(std::shared_ptr<ov::Data> &data_stream, const std::vector<std::shared_ptr<const SampleData>> &sample_datas)
{
	auto data = std::make_shared<ov::Data>();
	uint32_t flag = 0;

	if (M4sMediaType::Video == _media_type)
	{
		flag = TRUN_FLAG_DATA_OFFSET_PRESENT | TRUN_FLAG_SAMPLE_DURATION_PRESENT | TRUN_FLAG_SAMPLE_SIZE_PRESENT |
			   TRUN_FLAG_SAMPLE_FLAGS_PRESENT | TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT;
	}
	else if (M4sMediaType::Audio == _media_type)
	{
		flag = TRUN_FLAG_DATA_OFFSET_PRESENT | TRUN_FLAG_SAMPLE_DURATION_PRESENT | TRUN_FLAG_SAMPLE_SIZE_PRESENT;
	}

	WriteUint32(sample_datas.size(), data);  // Sample Item Count;
	WriteUint32(0x11111111, data);			 // Data offset - temp 0 setting

	for (auto &sample_data : sample_datas)
	{
		WriteUint32(sample_data->duration, data);  // duration

		if (_media_type == M4sMediaType::Video)
		{
			WriteUint32(sample_data->data->GetLength() + 4, data);	// size + sample
			WriteUint32(sample_data->flag, data);					  // flag
			WriteUint32(sample_data->composition_time_offset, data);  // compoistion timeoffset
		}
		else if (_media_type == M4sMediaType::Audio)
		{
			WriteUint32(sample_data->data->GetLength(), data);  // sample
		}
	}

	return WriteBoxData("trun", 0, flag, data, data_stream);
}

int M4sSegmentWriter::WriteMdatBox(std::shared_ptr<ov::Data> &data_stream, const std::vector<std::shared_ptr<const SampleData>> &sample_datas, uint32_t total_sample_size)
{
	WriteUint32(MP4_BOX_HEADER_SIZE + total_sample_size, data_stream);  // box size write
	WriteText("mdat", data_stream);										// type write

	for (auto &sample_data : sample_datas)
	{
		// only video)
		if (_media_type == M4sMediaType::Video)
		{
			WriteUint32(sample_data->data->GetLength(), data_stream);
		}

		WriteData(sample_data->data, data_stream);
	}

	return data_stream->GetLength();
}