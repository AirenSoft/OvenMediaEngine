//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_stream_packetyzer.h"
#include "hls_private.h"

//====================================================================================================
// Constructor
//====================================================================================================
HlsStreamPacketyzer::HlsStreamPacketyzer(int segment_count,
                                       int segment_duration,
                                       std::string &segment_prefix,
                                       PacketyzerStreamType stream_type,
                                       PacketyzerMediaInfo media_info) :
                                       StreamPacketyzer(stream_type,
                                                       PACKTYZER_DEFAULT_TIMESCALE,
                                                        PACKTYZER_DEFAULT_TIMESCALE,
                                                       static_cast<uint32_t>(media_info.video_framerate))
{
    media_info.video_timescale = PACKTYZER_DEFAULT_TIMESCALE;
    media_info.audio_timescale = PACKTYZER_DEFAULT_TIMESCALE;

    _packetyzer = std::make_shared<HlsPacketyzer>(segment_prefix,
                                                  stream_type,
                                                  segment_count,
                                                  segment_duration,
                                                  media_info);
}

//====================================================================================================
// Destructor
//====================================================================================================
HlsStreamPacketyzer::~HlsStreamPacketyzer()
{
}

//====================================================================================================
// Append Video Frame
//====================================================================================================
bool HlsStreamPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
    _packetyzer->AppendVideoFrame(data);
    return true;
}

//====================================================================================================
// Append Audi Frame
//====================================================================================================
bool HlsStreamPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
    _packetyzer->AppendAudioFrame(data);
    return true;
}

//====================================================================================================
// Get PlayList
// - M3U8
//====================================================================================================
bool HlsStreamPacketyzer::GetPlayList(ov::String &segment_play_list)
{
    bool result = false;
    std::string play_list;

    result = _packetyzer->GetPlayList(play_list);

    if (result)
        segment_play_list = play_list.c_str();

    return result;
}

//====================================================================================================
// GetSegment
// - TS
//====================================================================================================
bool HlsStreamPacketyzer::GetSegment(const ov::String &segment_file_name, std::shared_ptr<ov::Data> &segment_data)
{
     return _packetyzer->GetSegmentData(SegmentDataType::Ts, segment_file_name, segment_data);
}
