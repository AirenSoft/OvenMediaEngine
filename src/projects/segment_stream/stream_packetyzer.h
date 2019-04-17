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

#define DEFAULT_SEGMENT_COUNT        (5)
#define DEFAULT_SEGMENT_DURATION    (5)

//====================================================================================================
// SegmentConfigInfo
//====================================================================================================
struct SegmentConfigInfo
{
public:
    SegmentConfigInfo(bool enable, int count, int duration)
    {
        _enable = enable;
        _count = count;
        _duration = duration;
    }

public:
    bool _enable;
    int _count;
    int _duration;
};

//====================================================================================================
// StreamPacketyzer
//====================================================================================================
class StreamPacketyzer
{
public:
    StreamPacketyzer(SegmentConfigInfo dash_segment_config_info,
                     SegmentConfigInfo hls_segment_config_info,
                     std::string &segment_prefix,
                     PacketyzerStreamType stream_type,
                     PacketyzerMediaInfo media_info);

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

    bool GetPlayList(PlayListType play_list_type, ov::String &segment_play_list);

    bool GetSegment(SegmentType type, const ov::String &file_name, std::shared_ptr<ov::Data> &data);

private :
    bool VideoDataSampleWrite(uint64_t timestamp);

private :
    std::shared_ptr<HlsPacketyzer> _hls_packetyzer = nullptr;
    std::shared_ptr<DashPacketyzer> _dash_packetyzer = nullptr;
    uint32_t _audio_timescale;
    uint32_t _video_timescale;
    PacketyzerStreamType _stream_type;
    time_t _start_time;
    uint32_t _video_framerate;
    std::deque<std::shared_ptr<PacketyzerFrameData>> _video_data_queue;

    uint64_t _last_video_timestamp = 0;
    uint64_t _last_audio_timestamp = 0;

};

