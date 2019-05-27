//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include "packetyzer.h"
#include "bit_writer.h"
#include <base/common_types.h>

//====================================================================================================
// TsWriter
//====================================================================================================

class TsWriter
{
public:
	TsWriter(bool video_enable, bool audio_enable);
	virtual ~TsWriter() = default;
	
public :
	bool WriteSample(bool is_video,
                   bool is_keyframe,
                   uint64_t timestamp,
                   uint64_t time_offset,
                   std::shared_ptr<ov::Data> &frame_data);

	const std::shared_ptr<std::vector<uint8_t>> &GetDataStream(){ return _data_stream; };

protected : 	
	static uint32_t	MakeCrc(const uint8_t * data, uint32_t data_size);
	bool WritePAT();
	bool WritePMT();
	bool MakePesHeader(int data_size,
                        bool is_video,
                        uint64_t timestamp,
                        uint64_t time_offset,
                        uint8_t * header,
                        uint32_t & header_size);

	bool MakeTsHeader(int pid,
                        uint32_t continuity_count,
                        bool payload_start,
                        uint32_t & payload_size,
                        bool use_pcr,
                        uint64_t pcr,
                        bool is_keyframe);

	bool WriteDataStream(int data_size, const uint8_t * dat);

protected :
    bool _video_enable;
    bool _audio_enable;

    std::shared_ptr<std::vector<uint8_t>> _data_stream;
	uint32_t _audio_continuity_count;
	uint32_t _video_continuity_count;


};