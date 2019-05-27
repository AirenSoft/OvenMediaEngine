//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include <map>
#include "../base/ovlibrary/ovlibrary.h"
#include "packetyzer_define.h"

#define MPD_AUDIO_SUFFIX            "_audio.m4s"
#define MPD_VIDEO_SUFFIX            "_video.m4s"
#define MPD_VIDEO_INIT_FILE_NAME    "init_video.m4s"
#define MPD_AUDIO_INIT_FILE_NAME    "init_audio.m4s"

//====================================================================================================
// Packetyzer
//====================================================================================================
class Packetyzer
{
public:
    Packetyzer(const ov::String &app_name,
               const ov::String &stream_name,
               PacketyzerType packetyzer_type,
               PacketyzerStreamType stream_type,
               const ov::String &segment_prefix,
               uint32_t segment_count,
               uint32_t segment_duration,
               PacketyzerMediaInfo &media_info);

    virtual ~Packetyzer();

public :
    virtual bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) = 0;

    virtual bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) = 0;

    virtual bool GetSegmentData(const ov::String &file_name, std::shared_ptr<ov::Data> &data) = 0;

    virtual bool SetSegmentData(ov::String file_name,
                                uint64_t duration,
                                uint64_t timestamp_,
                                const std::shared_ptr<std::vector<uint8_t>> &data) = 0;

    static uint64_t ConvertTimeScale(uint64_t time, uint32_t from_timescale, uint32_t to_timescale);

    void SetPlayList(ov::String &play_list);

    bool GetPlayList(ov::String &play_list);

    bool GetVideoPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas);

    bool GetAudioPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas);

    static uint32_t Gcd(uint32_t n1, uint32_t n2);
    static std::string MakeUtcTimeString(time_t value);
    static double GetCurrentMilliseconds();

protected :
    ov::String _app_name;
    ov::String _stream_name;
    PacketyzerType _packetyzer_type;
    ov::String _segment_prefix;
    PacketyzerStreamType _stream_type;

    uint32_t _segment_count;
    uint32_t _segment_save_count;
    uint64_t _segment_duration; // second
    PacketyzerMediaInfo _media_info;

    uint32_t _sequence_number;
    bool _init_segment_count_complete;
    ov::String _play_list;

    bool _video_init;
    bool _audio_init;

    uint32_t _current_video_index = 0;
    uint32_t _current_audio_index = 0;

    std::vector<std::shared_ptr<SegmentData>> _video_segment_datas; // m4s : video , ts : video+audio
    std::vector<std::shared_ptr<SegmentData>> _audio_segment_datas; // m4s : audio

    std::mutex _video_segment_guard;
    std::mutex _audio_segment_guard;
    std::mutex _play_list_guard;
};
