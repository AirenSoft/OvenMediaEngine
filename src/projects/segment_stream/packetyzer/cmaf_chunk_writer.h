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
// CmafChunkWriter
//====================================================================================================
class CmafChunkWriter : public M4sWriter
{
public:
	CmafChunkWriter(M4sMediaType media_type,
						  uint32_t sequence_number,
						  uint32_t track_id,
						  bool http_chunked_transfer_support);

	~CmafChunkWriter() final;

public :
	int AppendSamples(const std::vector<std::shared_ptr<SampleData>> &sample_datas);
    const std::shared_ptr<ov::Data> AppendSample(const std::shared_ptr<SampleData> &sample_data);

    uint32_t GetSequenceNumber(){ return _sequence_number; }
    uint64_t GetStartTimestamp(){ return _start_timestamp; }
    const std::vector<std::shared_ptr<ov::Data>> &GetChunkDataList(){ return _chunk_data_list; }
    bool IsWriteStarted(){ return _write_started; }
    void Clear()
    {
        _write_started = false;
        _start_timestamp = 0;
		_chunk_data_list.clear();
		_total_chunk_data_size = 0;
    }

	 std::shared_ptr<ov::Data> GetChunkedSegment();

protected :

	int MoofBoxWrite(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<SampleData> &sample_data);
	int MfhdBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int TrafBoxWrite(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<SampleData> &sample_data);

	int TfhdBoxWrite(std::shared_ptr<ov::Data> &data_stream);
	int TfdtBoxWrite(std::shared_ptr<ov::Data> &data_stream, uint64_t timestamp);
	int TrunBoxWrite(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<SampleData> &sample_data);

	int MdatBoxWrite(std::shared_ptr<ov::Data> &data_stream, const std::shared_ptr<ov::Data> &frame_data);

private :
	std::vector<std::shared_ptr<ov::Data>> _chunk_data_list;
	uint32_t _total_chunk_data_size;
	bool _http_chunked_transfer_support = true;

	uint32_t _sequence_number;
	uint32_t _track_id;

	bool _write_started = false;
	uint64_t _start_timestamp = 0;
};