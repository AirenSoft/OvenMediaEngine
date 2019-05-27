//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include "segment_stream/segment_stream.h"
#include "hls_stream_packetyzer.h"

//====================================================================================================
// HlsStream
//====================================================================================================
class HlsStream : public SegmentStream
{
public:
    static std::shared_ptr<HlsStream> Create(int segment_count,
                                             int segment_duration,
                                             const std::shared_ptr<Application> application,
                                             const StreamInfo &info,
                                             uint32_t worker_count);

    HlsStream(const std::shared_ptr<Application> application, const StreamInfo &info);

	~HlsStream();

    std::shared_ptr<StreamPacketyzer> CreateStreamPacketyzer(int segment_count,
                                                             int segment_duration,
                                                             const ov::String &segment_prefix,
                                                             PacketyzerStreamType stream_type,
                                                             PacketyzerMediaInfo media_info) override
    {
        auto stream_packetyzer = std::make_shared<HlsStreamPacketyzer>(GetApplication()->GetName(),
                                                                        GetName(),
                                                                        segment_count,
                                                                        segment_duration,
                                                                        segment_prefix,
                                                                        stream_type,
                                                                        media_info);

        return std::static_pointer_cast<StreamPacketyzer>(stream_packetyzer);
    }

private:

};
