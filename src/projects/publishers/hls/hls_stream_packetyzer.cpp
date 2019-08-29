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
                                        std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track) :
                                        StreamPacketyzer(app_name,
                                                        stream_name,
                                                        segment_count,
                                                        segment_duration,
                                                        stream_type,
                                                        video_track, audio_track)
{
    _packetyzer = std::make_shared<HlsPacketyzer>(app_name,
                                                stream_name,
                                                stream_type,
                                                segment_prefix,
                                                segment_count,
                                                segment_duration,
                                                video_track, audio_track);
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
// GetSegmentData
// - TS
//====================================================================================================
std::shared_ptr<SegmentData> HlsStreamPacketyzer::GetSegmentData(const ov::String &file_name)
{
     return _packetyzer->GetSegmentData(file_name);
}
