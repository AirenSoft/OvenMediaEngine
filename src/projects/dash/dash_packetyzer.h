//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "segment_stream/packetyzer/packetyzer.h"
#include "segment_stream/packetyzer/m4s_init_writer.h"
#include "segment_stream/packetyzer/m4s_fragment_writer.h"

//====================================================================================================
// DashPacketyzer
// m4s : [Prefix]_[Index].TS
// mpd : manifest.mpd
//====================================================================================================
class DashPacketyzer : public Packetyzer
{
public:
    DashPacketyzer(std::string &segment_prefix,
                   PacketyzerStreamType stream_type,
                   uint32_t segment_count,
                   uint32_t segment_duration,
                   PacketyzerMediaInfo &media_info);

    ~DashPacketyzer() final;

public :
    bool VideoInit(std::shared_ptr<std::vector<uint8_t>> &data);

    bool AudioInit();

    bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data);

    bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data);

    bool VideoSegmentWrite(uint64_t last_timestamp);

    bool AudioSegmentWrite(uint64_t last_timestamp);

protected :
    bool UpdatePlayList(bool video_update);

private :
    int _avc_nal_header_size;
    std::string _start_time;
    std::string _mpd_pixel_aspect_ratio;
    double _mpd_suggested_presentation_delay;
    double _mpd_min_buffer_time;
    double _mpd_update_time;

    std::deque<std::shared_ptr<PacketyzerFrameData>> _video_frame_datas;
    std::deque<std::shared_ptr<PacketyzerFrameData>> _audio_frame_datas;

    double _duration_threshold;
};