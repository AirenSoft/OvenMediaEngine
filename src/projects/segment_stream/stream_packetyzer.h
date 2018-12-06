//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include <string>
#include <deque>
#include <base/ovlibrary/ovlibrary.h>
#include "packetyzer/hls_packetyzer.h"
#include "packetyzer/dash_packetyzer.h"

//====================================================================================================
// StreamPacketyzer
//====================================================================================================
class StreamPacketyzer
{
public:
	StreamPacketyzer(std::string &segment_prefix, PacketyzerStreamType stream_type, int segment_count, int segment_duration, PacketyzerMediaInfo &media_info);
	virtual ~StreamPacketyzer();

public :
    bool    AppendVideoData(uint64_t timestamp, uint32_t timescale, bool is_keyframe, uint64_t time_offset, uint32_t data_size, uint8_t *data);
	bool    AppendAudioData(uint64_t timestamp, uint32_t timescale, uint32_t data_size, uint8_t *data);

	bool    GetPlayList(PlayListType play_list_type, ov::String &segment_play_list);
	bool    GetSegment(SegmentType type, const ov::String &file_name, std::shared_ptr<ov::Data> &data);

private : 
	bool    VideoDataSampleWrite(uint64_t timestamp);

private :
	std::shared_ptr<HlsPacketyzer>  _hls_packetyzer;
	std::shared_ptr<DashPacketyzer> _dash_packetyzer;
	uint32_t 						_audio_timescale;
	uint32_t 						_video_timescale;
    PacketyzerStreamType            _stream_type;
	std::deque<std::shared_ptr<PacketyzerFrameData>>	_video_data_queue;
};

