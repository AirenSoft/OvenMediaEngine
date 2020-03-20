//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include <publishers/segment/segment_stream/packetizer/ts_writer.h>

//====================================================================================================
// HlsPacketizer
// TS : [Prefix]_[Index].TS
// M3U8 : playlist.M3U8
//====================================================================================================
class HlsPacketizer : public Packetizer
{
public:
	HlsPacketizer(const ov::String &app_name,
                const ov::String &stream_name,
                PacketizerStreamType stream_type,
                const ov::String &segment_prefix,
                uint32_t segment_count,
                uint32_t segment_duration,
                std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track);

	~HlsPacketizer() = default;

    virtual const char *GetPacketizerName() const
	{
		return "HLS";
	}
	
public :
    virtual bool AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &frame_data) override;

    virtual bool AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &frame_data) override;

    virtual const std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name) override;

    virtual bool SetSegmentData(ov::String file_name,
								uint64_t duration,
								int64_t timestamp,
								std::shared_ptr<ov::Data> &data) override;

    bool SegmentWrite(int64_t start_timestamp, uint64_t duration);

protected : 	
	bool UpdatePlayList();
	
protected :
	std::vector<std::shared_ptr<PacketizerFrameData>> _frame_datas;
    double _duration_margin;

    time_t _last_video_append_time;
    time_t _last_audio_append_time;
    bool _audio_enable;
    bool _video_enable;

    ov::StopWatch _stat_stop_watch;
};

