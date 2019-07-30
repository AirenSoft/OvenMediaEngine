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
#include "segment_stream/packetyzer/m4s_segment_writer.h"

enum class DashFileType : int32_t
{
	Unknown,
	PlayList,
	VideoInit,
	AudioInit,
	VideoSegment,
	AudioSegment,
};

//====================================================================================================
// DashPacketyzer
// m4s : [Prefix]_[Index]_[Suffix].m4s
// mpd : manifest.mpd
//====================================================================================================
class DashPacketyzer : public Packetyzer
{
public:
    DashPacketyzer(const ov::String &app_name,
                    const ov::String &stream_name,
                    PacketyzerStreamType stream_type,
                    const ov::String &segment_prefix,
                    uint32_t segment_count,
                    uint32_t segment_duration,
                    PacketyzerMediaInfo &media_info);

    ~DashPacketyzer() final;

public :

	static DashFileType GetFileType(const ov::String &file_name);

    bool VideoInit(const std::shared_ptr<ov::Data> &frame_data);

    bool AudioInit();

    virtual bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) override;

    virtual bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) override;

    virtual const std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name) override;

    virtual bool SetSegmentData(ov::String file_name,
								uint64_t duration,
								uint64_t timestamp,
								std::shared_ptr<ov::Data> &data) override;

    bool VideoSegmentWrite(uint64_t max_timestamp);

    bool AudioSegmentWrite(uint64_t max_timestamp);

protected :

    bool GetSegmentPlayInfos(ov::String & video_urls,
                            ov::String &audio_urls,
                            double &time_shift_buffer_depth,
                            double &minimumUpdatePeriod);

    bool UpdatePlayList();

private :
    int _avc_nal_header_size;
	ov::String _start_time;
    std::string _mpd_pixel_aspect_ratio;
    double _mpd_suggested_presentation_delay;
    double _mpd_min_buffer_time;

    std::shared_ptr<SegmentData> _video_init_file = nullptr;
    std::shared_ptr<SegmentData> _audio_init_file = nullptr;

    uint32_t _video_sequence_number = 1;
    uint32_t _audio_sequence_number = 1;

    std::deque<std::shared_ptr<PacketyzerFrameData>> _video_frame_datas;
    std::deque<std::shared_ptr<PacketyzerFrameData>> _audio_frame_datas;

    time_t _last_video_append_time;
    time_t _last_audio_append_time;

    double _duration_margen;
};