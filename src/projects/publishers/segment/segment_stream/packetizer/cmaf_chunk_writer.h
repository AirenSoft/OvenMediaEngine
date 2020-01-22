//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "m4s_writer.h"
#include "packetizer_define.h"

class CmafChunkWriter : public M4sWriter
{
public:
	CmafChunkWriter(M4sMediaType media_type, uint32_t track_id, double ideal_duration);

	const std::shared_ptr<ov::Data> AppendSample(const std::shared_ptr<const SampleData> &sample_data);

	uint32_t GetSequenceNumber() const
	{
		return _sequence_number;
	}

	int64_t GetStartTimestamp() const
	{
		return _start_timestamp;
	}

	uint32_t GetSampleCount() const
	{
		return _sample_count;
	}

	uint64_t GetSegmentDuration() const;

	bool IsWriteStarted() const
	{
		return _write_started;
	}

	void Clear()
	{
		_write_started = false;
		_start_timestamp = 0LL;
		_sample_count = 0U;
		_chunked_data = nullptr;
		_last_sample = nullptr;
	}

	std::shared_ptr<ov::Data> GetChunkedSegment();

protected:
	int WriteMoofBox(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<const SampleData> &sample_data);
	int WriteMfhdBox(std::shared_ptr<ov::Data> &data_stream);
	int WriteTrafBox(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<const SampleData> &sample_data);
	int WriteTfhdBox(std::shared_ptr<ov::Data> &data_stream);
	int WriteTfdtBox(std::shared_ptr<ov::Data> &data_stream, int64_t timestamp);
	int WriteTrunBox(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<const SampleData> &sample_data);

	int WriteMdatBox(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<ov::Data> &frame_data);

private:
	uint32_t _max_chunked_data_size = 100 * 1024;

	uint32_t _sequence_number = 0U;
	uint32_t _track_id;
	double _ideal_duration = 0.0;

	bool _write_started = false;
	int64_t _start_timestamp = 0LL;
	uint32_t _sample_count = 0U;
	std::shared_ptr<ov::Data> _chunked_data = nullptr;
	std::shared_ptr<const SampleData> _last_sample;
};