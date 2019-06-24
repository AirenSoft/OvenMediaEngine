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
#include "segment_stream/packetyzer/cmaf_m4s_fragment_writer.h"

//====================================================================================================
// CmafPacketyzer
// m4s : [Prefix]_[Index]_[Suffix].m4s
// mpd : manifest.mpd
//====================================================================================================
class CmafPacketyzer : public Packetyzer
{
public:
    CmafPacketyzer(const ov::String &app_name,
                    const ov::String &stream_name,
                    PacketyzerStreamType stream_type,
                    const ov::String &segment_prefix,
                    uint32_t segment_count,
                    uint32_t segment_duration,
                    PacketyzerMediaInfo &media_info);

    ~CmafPacketyzer() final;

public :
    bool VideoInit(const std::shared_ptr<PacketyzerFrameData> &frame_data);

    bool AudioInit();

    virtual bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) override;

    virtual bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) override;

    bool VideoSegmentWrite(uint64_t max_timestamp);

    bool AudioSegmentWrite(uint64_t max_timestamp);

    virtual bool GetSegmentData(const ov::String &file_name, std::shared_ptr<ov::Data> &data) override;

    virtual bool SetSegmentData(ov::String file_name,
                                uint64_t duration,
                                uint64_t timestamp,
                                const std::shared_ptr<std::vector<uint8_t>> &data) override;
protected :
    bool UpdatePlayList();

private :
    int _avc_nal_header_size;

    std::string _start_time_string;
    double _start_time = 0; //ms
    std::string _mpd_pixel_aspect_ratio;
    double _mpd_suggested_presentation_delay;
    double _mpd_min_buffer_time;

    std::shared_ptr<SegmentData> _mpd_video_init_file = nullptr;
    std::shared_ptr<SegmentData> _mpd_audio_init_file = nullptr;

    uint32_t _video_sequence_number = 1;
    uint32_t _audio_sequence_number = 1;

    std::shared_ptr<PacketyzerFrameData> _previous_video_frame = nullptr;
    std::shared_ptr<PacketyzerFrameData> _previous_audio_frame = nullptr;
    std::unique_ptr<CmafM4sFragmentWriter> _video_segment_writer = nullptr;
    std::unique_ptr<CmafM4sFragmentWriter> _audio_segment_writer = nullptr;
};