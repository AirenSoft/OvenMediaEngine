//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_stream.h"
#include "cmaf_private.h"

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<CmafStream> CmafStream::Create(int segment_count,
                                              int segment_duration,
                                              const std::shared_ptr<Application> application,
                                              const StreamInfo &info,
                                              uint32_t worker_count)
{
    auto stream = std::make_shared<CmafStream>(application, info);

    if (!stream->Start(segment_count, segment_duration, 0))
    {
        return nullptr;
    }
    return stream;
}


//====================================================================================================
// SegmentStream
// -  H264/AAC only
// TODO : 다중 트랜스코딩/다중 트랙 구분 및 처리 필요
//====================================================================================================
CmafStream::CmafStream(const std::shared_ptr<Application> application, const StreamInfo &info)
                    : SegmentStream(application, info)
{

}

CmafStream::~CmafStream()
{

}
