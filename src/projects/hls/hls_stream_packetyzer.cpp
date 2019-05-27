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
HlsStreamPacketyzer::HlsStreamPacketyzer(const ov::String &app_name,
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
                                                        PACKTYZER_DEFAULT_TIMESCALE,
                                                        static_cast<uint32_t>(media_info.video_framerate))
{
    media_info.video_timescale = PACKTYZER_DEFAULT_TIMESCALE;
    media_info.audio_timescale = PACKTYZER_DEFAULT_TIMESCALE;

    _packetyzer = std::make_shared<HlsPacketyzer>(app_name,
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
HlsStreamPacketyzer::~HlsStreamPacketyzer()
{
}

//====================================================================================================
// Append Video Frame
//====================================================================================================
bool HlsStreamPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
    return _packetyzer->AppendVideoFrame(data);
}

//====================================================================================================
// Append Audi Frame
//====================================================================================================
bool HlsStreamPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
    return _packetyzer->AppendAudioFrame(data);
}

//====================================================================================================
// Get PlayList
// - M3U8
//====================================================================================================
bool HlsStreamPacketyzer::GetPlayList(ov::String &play_list)
{
    return _packetyzer->GetPlayList(play_list);
}

//====================================================================================================
// GetSegment
// - TS
//====================================================================================================
bool HlsStreamPacketyzer::GetSegment(const ov::String &segment_file_name, std::shared_ptr<ov::Data> &segment_data)
{
     return _packetyzer->GetSegmentData(segment_file_name, segment_data);
}
