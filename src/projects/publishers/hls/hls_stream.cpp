//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_stream.h"
#include "hls_private.h"

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<HlsStream> HlsStream::Create(int segment_count,
                                             int segment_duration,
                                             const std::shared_ptr<Application> application,
                                             const StreamInfo &info,
                                             uint32_t worker_count)
{
    auto stream = std::make_shared<HlsStream>(application, info);

    if (!stream->Start(segment_count, segment_duration, 0))
    {
        return nullptr;
    }
    return stream;
}

//====================================================================================================
// SegmentStream
// - DASH/HLS : H264/AAC only
// TODO : 다중 트랜스코딩/다중 트랙 구분 및 처리 필요
//====================================================================================================
HlsStream::HlsStream(const std::shared_ptr<Application> application, const StreamInfo &info)
                    : SegmentStream(application, info)
{

}

HlsStream::~HlsStream()
{

}