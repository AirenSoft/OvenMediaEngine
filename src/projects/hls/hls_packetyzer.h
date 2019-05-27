//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include "segment_stream/packetyzer/ts_writer.h"

//====================================================================================================
// HlsPacketyzer
// TS : [Prefix]_[Index].TS
// M3U8 : playlist.M3U8
//====================================================================================================
class HlsPacketyzer : public Packetyzer
{
public:
	HlsPacketyzer(const ov::String &app_name,
                const ov::String &stream_name,
                PacketyzerStreamType stream_type,
                const ov::String &segment_prefix,
                uint32_t segment_count,
                uint32_t segment_duration,
                PacketyzerMediaInfo &media_info);

	~HlsPacketyzer() = default;
	
public :
    virtual bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) override;

    virtual bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) override;

    virtual bool GetSegmentData(const ov::String &file_name, std::shared_ptr<ov::Data> &data) override;

    virtual bool SetSegmentData(ov::String file_name,
                                uint64_t duration,
                                uint64_t timestamp_,
                                const std::shared_ptr<std::vector<uint8_t>> &data) override;

    bool SegmentWrite(uint64_t start_timestamp, uint64_t duration);

protected : 	
	bool UpdatePlayList();
	
protected :
	std::vector<std::shared_ptr<PacketyzerFrameData>> _frame_datas;
    double _duration_threshold;

    time_t _last_video_append_time;
    time_t _last_audio_append_time;
    time_t _audio_enable;
    time_t _video_enable;

};

