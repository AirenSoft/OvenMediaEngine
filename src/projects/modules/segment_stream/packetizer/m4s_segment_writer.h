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

class M4sSegmentWriter : public M4sWriter
{
public:
	M4sSegmentWriter(M4sMediaType media_type, uint32_t sequence_number, uint32_t track_id, int64_t start_timestamp);
	~M4sSegmentWriter() final;

	const std::shared_ptr<ov::Data> AppendSamples(const std::vector<std::shared_ptr<const SampleData>> &sample_datas);

protected:
	int WriteMoofBox(std::shared_ptr<ov::Data> &data_stream, const std::vector<std::shared_ptr<const SampleData>> &sample_datas);
	int WriteMfhdBox(std::shared_ptr<ov::Data> &data_stream);
	int WriteTrafBox(std::shared_ptr<ov::Data> &data_stream, const std::vector<std::shared_ptr<const SampleData>> &sample_datas);
	int WriteTfhdBox(std::shared_ptr<ov::Data> &data_stream);
	int WriteTfdtBox(std::shared_ptr<ov::Data> &data_stream);
	int WriteTrunBox(std::shared_ptr<ov::Data> &data_stream, const std::vector<std::shared_ptr<const SampleData>> &sample_datas);
	int WriteMdatBox(std::shared_ptr<ov::Data> &data_stream, const std::vector<std::shared_ptr<const SampleData>> &sample_datas, uint32_t total_sample_size);

private:
	uint32_t _sequence_number = 0U;
	uint32_t _track_id = 0U;
	int64_t _start_timestamp = 0LL;
};