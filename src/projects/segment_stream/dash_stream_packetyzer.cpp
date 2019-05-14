//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dash_stream_packetyzer.h"
#include "segment_stream_private.h"

//====================================================================================================
// Constructor
//====================================================================================================
DashStreamPacketyzer::DashStreamPacketyzer(int segment_count,
                                            int segment_duration,
                                            std::string &segment_prefix,
                                            PacketyzerStreamType stream_type,
                                            PacketyzerMediaInfo media_info) :
                                            StreamPacketyzer(stream_type,
                                                     PACKTYZER_DEFAULT_TIMESCALE,
                                                     media_info.audio_samplerate,
                                                     static_cast<uint32_t>(media_info.video_framerate))
{
    logtd("Dash Packetyzer Create - count(%d) duration(%d)");

    media_info.video_timescale = PACKTYZER_DEFAULT_TIMESCALE;
    media_info.audio_timescale = media_info.audio_samplerate;

    _packetyzer = std::make_shared<DashPacketyzer>(segment_prefix,
                                                    stream_type,
                                                    segment_count,
                                                    segment_duration,
                                                    media_info);
}

//====================================================================================================
// Destructor
//====================================================================================================
DashStreamPacketyzer::~DashStreamPacketyzer()
{
}

//====================================================================================================
// Append Video Frame
//====================================================================================================
bool DashStreamPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
    _packetyzer->AppendVideoFrame(data);
    return true;
}

//====================================================================================================
// Append Audi Frame
//====================================================================================================
bool DashStreamPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
    _packetyzer->AppendAudioFrame(data);
    return true;
}

//====================================================================================================
// Get PlayList
// - MPD
//====================================================================================================
bool DashStreamPacketyzer::GetPlayList(ov::String &segment_play_list)
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
// - MP4
//====================================================================================================
bool DashStreamPacketyzer::GetSegment(const ov::String &segment_file_name, std::shared_ptr<ov::Data> &segment_data)
{
    bool result = false;

    if(segment_file_name.IndexOf(MPD_AUDIO_SUFFIX) >= 0)
        result = _packetyzer->GetSegmentData(SegmentDataType::Mp4Audio, segment_file_name, segment_data);
    else if(segment_file_name.IndexOf(MPD_VIDEO_SUFFIX) >= 0)
        result = _packetyzer->GetSegmentData(SegmentDataType::Mp4Video, segment_file_name, segment_data);

    return result;
}
