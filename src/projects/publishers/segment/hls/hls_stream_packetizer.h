//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <publishers/segment/segment_stream/stream_packetizer.h>
#include "hls_packetizer.h"

//====================================================================================================
// HlsStreamPacketizer
//====================================================================================================
class HlsStreamPacketizer : public StreamPacketizer
{
public:
    HlsStreamPacketizer(const ov::String &app_name,
                        const ov::String &stream_name,
                        int segment_count,
                        int segment_duration,
                        const ov::String &segment_prefix,
                        PacketizerStreamType stream_type,
                        std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track);

    virtual ~HlsStreamPacketizer();

public :

    // Implement StreamPacketizer Interface
    bool AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &data) override;
    bool AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &data) override;
    bool GetPlayList(ov::String &play_list) override;
	std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name) override;

private :

};

