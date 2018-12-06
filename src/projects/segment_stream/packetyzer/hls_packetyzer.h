//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include "ts_writer.h"

//====================================================================================================
// HlsPacketyzer
// TS : [Prefix]_[Index].TS
// M3U8 : playlist.M3U8
//====================================================================================================
class HlsPacketyzer : public Packetyzer
{
public:
    HlsPacketyzer(	std::string				&segment_prefix,
					PacketyzerStreamType 	stream_type,
					uint32_t 				segment_count,
					uint32_t				segment_duration,
				    PacketyzerMediaInfo		&media_info);
	~HlsPacketyzer() = default;
	
public :
	bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData>  &frame_data);
	bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData>  &frame_data);
	bool SegmentWrite(uint64_t start_timestamp, uint64_t duration);

protected : 	
	bool UpdatePlayList();
	
protected :
	std::vector<std::shared_ptr<PacketyzerFrameData>> _frame_datas;
};

