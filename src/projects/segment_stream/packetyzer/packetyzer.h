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
    Packetyzer(PacketyzerType packetyzer_type,
               std::string &segment_prefix,
               PacketyzerStreamType stream_type,
               uint32_t segment_count,
               uint32_t segment_duration,
               PacketyzerMediaInfo &media_info);

    virtual ~Packetyzer();

public :
    static uint64_t ConvertTimeScale(uint64_t time, uint32_t from_timescale, uint32_t to_timescale);

    bool SetPlayList(std::string &play_list);

    bool SetSegmentData(SegmentDataType data_type,
                        uint32_t sequence_number,
                        std::string file_name,
                        uint64_t duration,
                        uint64_t timestamp_,
                        const std::shared_ptr<std::vector<uint8_t>> &data);

    bool GetPlayList(std::string &play_list);

    bool GetSegmentData(SegmentDataType data_type,
                        const ov::String &file_name,
                        std::shared_ptr<ov::Data> &data);

    bool GetVideoPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas);
    bool GetAudioPlaySegments(std::vector<std::shared_ptr<SegmentData>> &segment_datas);

    static uint32_t Gcd(uint32_t n1, uint32_t n2);
    static std::string MakeUtcTimeString(time_t value);
    static double GetCurrentMilliseconds();

protected :
    PacketyzerType _packetyzer_type;
    std::string _segment_prefix;
    PacketyzerStreamType _stream_type;
    int _segment_count;
    int _segment_save_count;
    uint64_t _segment_duration; // second
    PacketyzerMediaInfo _media_info;
    uint32_t _sequence_number;
    uint32_t _video_sequence_number;
    uint32_t _audio_sequence_number;
    bool _save_file;
    std::string _play_list;
    bool _video_init;
    bool _audio_init;
    bool _init_segment_count_complete;

    std::shared_ptr<SegmentData> _mpd_video_init_file = nullptr;
    std::shared_ptr<SegmentData> _mpd_audio_init_file = nullptr;

    int _current_video_index = 0;
    int _current_audio_index = 0;
    std::vector<std::shared_ptr<SegmentData>> _video_segment_datas; // m4s : video , ts : video+audio
    std::vector<std::shared_ptr<SegmentData>> _audio_segment_datas; // m4s : audio
};
