//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "segment_stream_server.h"

//====================================================================================================
// HlsStreamServer
//====================================================================================================
class HlsStreamServer : public SegmentStreamServer
{
public :
    HlsStreamServer() = default;

    ~HlsStreamServer() = default;

public :
    cfg::PublisherType GetPublisherType() override
    {
        return cfg::PublisherType::Hls;
    }

protected:

    void ProcessRequestStream(const std::shared_ptr<HttpRequest> &request,
                           const std::shared_ptr<HttpResponse> &response,
                           const ov::String &app_name,
                           const ov::String &stream_name,
                           const ov::String &file_name,
                           const ov::String &file_ext) override;

protected :
};
