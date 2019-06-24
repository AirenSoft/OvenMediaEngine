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
// M4sFragmentWriter
//====================================================================================================
class M4sFragmentWriter : public M4sWriter
{
public:
	M4sFragmentWriter(M4sMediaType media_type, uint32_t sequence_number, uint32_t track_id, uint64_t start_timestamp);

	~M4sFragmentWriter() final;

public :
	int AppendSamples(std::vector<std::shared_ptr<FragmentSampleData>> &sample_datas);
    const std::shared_ptr<std::vector<uint8_t>> &GetDataStream(){ return _data_stream; };
protected :

	int MoofBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int MfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TrafBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int TfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TfdtBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TrunBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
		
	int MdatBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
   
private :
    std::shared_ptr<std::vector<uint8_t>> _data_stream;
	uint32_t _sequence_number;
	uint32_t _track_id;
	uint64_t _start_timestamp;
	std::vector<std::shared_ptr<FragmentSampleData>> _sample_datas;
};