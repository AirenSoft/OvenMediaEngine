//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "m4s_writer.h"
#include "packetyzer_define.h"

//====================================================================================================
// Fragment Sample Data
//====================================================================================================
struct FragmentSampleData
{
public:
	FragmentSampleData(uint64_t duration_, uint32_t flag_, uint32_t composition_time_offset_, std::shared_ptr<ov::Data> &data_)
	{
		duration                =  duration_;
		flag                    = flag_;
		composition_time_offset = composition_time_offset_;
		data		            = data_;
	}

public:
	uint64_t duration;
	uint32_t flag;
	uint32_t composition_time_offset;
	std::shared_ptr<ov::Data> data;
};

//====================================================================================================
// M4sFragmentWriter
//====================================================================================================
class M4sFragmentWriter : public M4sWriter
{
public:
	M4sFragmentWriter(	M4sMediaType	media_type,
						uint32_t		data_init_size,
						uint32_t		sequence_number,
						uint32_t		track_id,
						uint64_t		start_timestamp,
						std::vector<std::shared_ptr<FragmentSampleData>> &sample_datas);

	~M4sFragmentWriter() final;

public :
	int CreateData();

protected :

	int MoofBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int MfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TrafBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int TfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TfdtBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TrunBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
		
	int MdatBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
   
private :
	uint32_t _sequence_number;
	uint32_t _track_id;
	uint64_t _start_timestamp;
	std::vector<std::shared_ptr<FragmentSampleData>> _sample_datas;
};