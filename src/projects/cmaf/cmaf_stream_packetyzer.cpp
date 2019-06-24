//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_stream_packetyzer.h"
#include "cmaf_private.h"

//====================================================================================================
// Constructor
//====================================================================================================
CmafStreamPacketyzer::CmafStreamPacketyzer(const ov::String &app_name,
                                           const ov::String &stream_name,
                                           int segment_count,
                                           int segment_duration,
                                           const ov::String &segment_prefix,
                                            PacketyzerStreamType stream_type,
                                            PacketyzerMediaInfo media_info) :
                                                StreamPacketyzer(app_name,
                                                                stream_name,
                                                                segment_count,
                                                                segment_duration,
                                                                stream_type,
                                                                PACKTYZER_DEFAULT_TIMESCALE,
                                                                media_info.audio_samplerate,
                                                                static_cast<uint32_t>(media_info.video_framerate))
{
    media_info.video_timescale = PACKTYZER_DEFAULT_TIMESCALE;
    media_info.audio_timescale = media_info.audio_samplerate;

    _packetyzer = std::make_shared<CmafPacketyzer>(app_name,
                                                stream_name,
                                                stream_type,
                                                segment_prefix,
                                                segment_count,
                                                segment_duration,
                                                media_info);
}

//====================================================================================================
// Destructor
//====================================================================================================
CmafStreamPacketyzer::~CmafStreamPacketyzer()
{
}

//====================================================================================================
// Append Video Frame
//====================================================================================================
bool CmafStreamPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
    return _packetyzer->AppendVideoFrame(data);
}

//====================================================================================================
// Append Audi Frame
//====================================================================================================
bool CmafStreamPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
    return _packetyzer->AppendAudioFrame(data);
}

//====================================================================================================
// Get PlayList
// - MPD
//====================================================================================================
bool CmafStreamPacketyzer::GetPlayList(ov::String &play_list)
{
   return _packetyzer->GetPlayList(play_list);
}

//====================================================================================================
// GetSegment
// - M4S
//====================================================================================================
bool CmafStreamPacketyzer::GetSegment(const ov::String &segment_file_name, std::shared_ptr<ov::Data> &segment_data)
{
      return _packetyzer->GetSegmentData(segment_file_name, segment_data);
}
