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
#include "packetyzer/packetyzer.h"

#define DEFAULT_SEGMENT_COUNT        (3)
#define DEFAULT_SEGMENT_DURATION    (5)

//====================================================================================================
// StreamPacketyzer
//====================================================================================================
class StreamPacketyzer
{
public:
    StreamPacketyzer(PacketyzerStreamType stream_type,
                     uint32_t video_timescale,
                     uint32_t auddio_timescale,
                     uint32_t video_framerate);

    virtual ~StreamPacketyzer();

public :
    bool
    AppendVideoData(uint64_t timestamp,
                    uint32_t timescale,
                    bool is_keyframe,
                    uint64_t time_offset,
                    uint32_t data_size,
                    const uint8_t *data);

    bool AppendAudioData(uint64_t timestamp, uint32_t timescale, uint32_t data_size, const uint8_t *data);

    // Child must implement this function
    virtual bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &data) = 0;
    virtual bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data)  = 0;
    virtual bool GetPlayList(ov::String &segment_play_list)  = 0;
    virtual bool GetSegment(const ov::String &file_name, std::shared_ptr<ov::Data> &data)  = 0;

private :
    bool VideoDataSampleWrite(uint64_t timestamp);

protected :
    uint32_t _audio_timescale;
    uint32_t _video_timescale;
    PacketyzerStreamType _stream_type;
    time_t _start_time;
    uint32_t _video_framerate;
    std::deque<std::shared_ptr<PacketyzerFrameData>> _video_data_queue;

    uint64_t _last_video_timestamp = 0;
    uint64_t _last_audio_timestamp = 0;

};

