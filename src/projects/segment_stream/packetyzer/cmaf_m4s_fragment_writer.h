//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "m4s_writer.h"
#include "packetyzer_define.h"

//====================================================================================================
// CmafM4sFragmentWriter
//====================================================================================================
class CmafM4sFragmentWriter : public M4sWriter
{
public:
    CmafM4sFragmentWriter(M4sMediaType media_type, uint32_t sequence_number, uint32_t track_id);

	~CmafM4sFragmentWriter() final;

public :
	int AppendSamples(const std::vector<std::shared_ptr<FragmentSampleData>> &sample_datas);
    std::shared_ptr<std::vector<uint8_t>> AppendSample(const std::shared_ptr<FragmentSampleData> &sample_data);

    uint32_t GetSequenceNumber(){ return _sequence_number; }
    uint64_t GetStartTimestamp(){ return _start_timestamp; }
    const std::shared_ptr<std::vector<uint8_t>> &GetSegment(){ return _data_stream; }
    bool IsWriteStarted(){ return _write_started; }
    void Clear()
    {
        _write_started = false;
        _start_timestamp = 0;
        _data_stream->clear();
    }

protected :

	int MoofBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream, const std::shared_ptr<FragmentSampleData> &sample_data);
	int MfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TrafBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream, const std::shared_ptr<FragmentSampleData> &sample_data);

	int TfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TfdtBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream, const std::shared_ptr<FragmentSampleData> &sample_data);
	int TrunBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream, const std::shared_ptr<FragmentSampleData> &sample_data);

	int MdatBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream, const std::shared_ptr<FragmentSampleData> &sample_data);

private :
    std::shared_ptr<std::vector<uint8_t>> _data_stream;
	uint32_t _sequence_number;
	uint32_t _track_id;

	bool _write_started = false;
	uint64_t _start_timestamp = 0;

};