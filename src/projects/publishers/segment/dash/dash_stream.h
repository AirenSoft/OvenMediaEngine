//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include <publishers/segment/segment_stream/segment_stream.h>
#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include "dash_stream_packetizer.h"

//====================================================================================================
// DashStream
//====================================================================================================
class DashStream : public SegmentStream
{
public:
    static std::shared_ptr<DashStream> Create(int segment_count,
                                               int segment_duration,
                                               const std::shared_ptr<pub::Application> application,
                                               const info::Stream &info,
                                               uint32_t worker_count);

	DashStream(const std::shared_ptr<pub::Application> application, const info::Stream &info);

	~DashStream();

    std::shared_ptr<StreamPacketizer> CreateStreamPacketizer(int segment_count,
                                                            int segment_duration,
                                                            const  ov::String &segment_prefix,
                                                            PacketizerStreamType stream_type,
                                                            std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track) override
    {
        auto stream_packetizer = std::make_shared<DashStreamPacketizer>(GetApplication()->GetName(),
                                                                        GetName(),
                                                                        segment_count,
                                                                        segment_duration,
                                                                        segment_prefix,
                                                                        stream_type,
                                                                        video_track, audio_track);

        return std::static_pointer_cast<StreamPacketizer>(stream_packetizer);
    }

private:

};
