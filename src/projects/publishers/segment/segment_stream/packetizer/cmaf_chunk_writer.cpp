//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_chunk_writer.h"

#define OV_LOG_TAG "CMAF.Writer"

#define DEFAULT_CHUNK_HEADER_SIZE (256)

CmafChunkWriter::CmafChunkWriter(M4sMediaType media_type, uint32_t track_id, double ideal_duration)
	: M4sWriter(media_type),
	  _track_id(track_id),
	  _ideal_duration(ideal_duration)
{
	OV_ASSERT2(_ideal_duration > 0.0);
}

std::shared_ptr<ov::Data> CmafChunkWriter::GetChunkedSegment()
{
	if (_chunked_data->GetLength() <= 0)
	{
		return nullptr;
	}

	auto chunked_segment = _chunked_data;

	if (_max_chunked_data_size < chunked_segment->GetLength())
	{
		_max_chunked_data_size = chunked_segment->GetLength();
	}

	return chunked_segment;
}

const std::shared_ptr<ov::Data> CmafChunkWriter::AppendSample(const std::shared_ptr<const SampleData> &sample_data)
{
	if (_write_started == false)
	{
		if (sample_data->timestamp < 0)
		{
			// OV_ASSERT2(false);
			return nullptr;
		}

		_write_started = true;
		_start_timestamp = sample_data->timestamp;

		if (_chunked_data == nullptr)
		{
			_chunked_data = std::make_shared<ov::Data>(_max_chunked_data_size);
		}

		// 0-based sequence number
		_sequence_number = (_start_timestamp / _ideal_duration);
		logtd("Calculated sequence number: %lld, %f = %u", _start_timestamp, _ideal_duration, _sequence_number);
	}

	auto chunk_stream = std::make_shared<ov::Data>(sample_data->data->GetLength() + DEFAULT_CHUNK_HEADER_SIZE);

	WriteMoofBox(chunk_stream, sample_data);
	WriteMdatBox(chunk_stream, sample_data->data);

	_chunked_data->Append(chunk_stream.get());

	_sample_count++;
	_last_sample = sample_data;

	return chunk_stream;
}

uint64_t CmafChunkWriter::GetSegmentDuration() const
{
	if (_last_sample == nullptr)
	{
		return 0ULL;
	}

	int64_t duration = (_last_sample->timestamp + _last_sample->duration) - _start_timestamp;

	OV_ASSERT((_last_sample->timestamp + static_cast<int64_t>(_last_sample->duration)) >= _start_timestamp, "%lld + %lld < %lld (duration: %lld)",
			  _last_sample->timestamp, _last_sample->duration,
			  _start_timestamp, duration);

	if (duration < 0LL)
	{
		logtw("Segment duration is negative (%lld + %lld < %lld, duration: %lld)",
			  _last_sample->timestamp, _last_sample->duration,
			  _start_timestamp, duration);

		duration = 0LL;
	}

	return duration;
}

int CmafChunkWriter::WriteMoofBox(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<const SampleData> &sample_data)
{
	auto data = std::make_shared<ov::Data>(DEFAULT_CHUNK_HEADER_SIZE);

	WriteMfhdBox(data);
	WriteTrafBox(data, sample_data);

	WriteBoxData("moof", data, data_stream);

	// trun data offset value change
	int position = (_media_type == M4sMediaType::Video) ? data_stream->GetLength() - 16 - 4 : data_stream->GetLength() - 8 - 4;

	if (position < 0)
	{
		return -1;
	}

	ByteWriter<uint32_t>::WriteBigEndian(data_stream->GetWritableDataAs<uint8_t>() + position, data_stream->GetLength() + 8);

	return data_stream->GetLength();
}

int CmafChunkWriter::WriteMfhdBox(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(_sequence_number, data);

	return WriteBoxData("mfhd", 0, 0, data, data_stream);
}

int CmafChunkWriter::WriteTrafBox(std::shared_ptr<ov::Data> &data_stream,
								  const std::shared_ptr<const SampleData> &sample_data)
{
	auto data = std::make_shared<ov::Data>();

	WriteTfhdBox(data);
	WriteTfdtBox(data, sample_data->timestamp);
	WriteTrunBox(data, sample_data);

	return WriteBoxData("traf", data, data_stream);
}

#define TFHD_FLAG_BASE_DATA_OFFSET_PRESENT (0x00001)
#define TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT (0x00002)
#define TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT (0x00008)
#define TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT (0x00010)
#define TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT (0x00020)
#define TFHD_FLAG_DURATION_IS_EMPTY (0x10000)
#define TFHD_FLAG_DEFAULT_BASE_IS_MOOF (0x20000)

int CmafChunkWriter::WriteTfhdBox(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();
	uint32_t flag = TFHD_FLAG_DEFAULT_BASE_IS_MOOF;

	WriteUint32(_track_id, data);  // track id

	return WriteBoxData("tfhd", 0, flag, data, data_stream);
}

int CmafChunkWriter::WriteTfdtBox(std::shared_ptr<ov::Data> &data_stream, int64_t timestamp)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint64(timestamp, data);  // Base media decode time

	return WriteBoxData("tfdt", 1, 0, data, data_stream);
}

#define TRUN_FLAG_DATA_OFFSET_PRESENT (0x0001)
#define TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT (0x0004)
#define TRUN_FLAG_SAMPLE_DURATION_PRESENT (0x0100)
#define TRUN_FLAG_SAMPLE_SIZE_PRESENT (0x0200)
#define TRUN_FLAG_SAMPLE_FLAGS_PRESENT (0x0400)
#define TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT (0x0800)

int CmafChunkWriter::WriteTrunBox(std::shared_ptr<ov::Data> &data_stream,
								  const std::shared_ptr<const SampleData> &sample_data)
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

	WriteUint32(1, data);			// Sample Item Count;
	WriteUint32(0x11111111, data);  // Data offset - temp 0 setting

	WriteUint32(sample_data->duration, data);  // duration

	if (_media_type == M4sMediaType::Video)
	{
		WriteUint32(sample_data->data->GetLength() + 4, data);	// size + sample
		WriteUint32(sample_data->flag, data);					  // flag
		WriteUint32(sample_data->composition_time_offset, data);  // cts
	}
	else if (_media_type == M4sMediaType::Audio)
	{
		WriteUint32(sample_data->data->GetLength(), data);  // sample
	}

	return WriteBoxData("trun", 0, flag, data, data_stream);
}

int CmafChunkWriter::WriteMdatBox(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<ov::Data> &frame_data)
{
	return WriteBoxData("mdat", frame_data, data_stream, _media_type == M4sMediaType::Video ? true : false);
}