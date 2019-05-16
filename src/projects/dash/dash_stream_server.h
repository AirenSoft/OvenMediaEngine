//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "segment_stream/segment_stream_server.h"
#include "dash_interceptor.h"

//====================================================================================================
// DashStreamServer
//====================================================================================================
class DashStreamServer : public SegmentStreamServer
{
public :
    DashStreamServer() = default;

    ~DashStreamServer() = default;

public :
    cfg::PublisherType GetPublisherType() override
    {
        return cfg::PublisherType::Dash;
    }

    std::shared_ptr<SegmentStreamInterceptor> CreateInterceptor() override
    {
       auto interceptor = std::make_shared<DashInterceptor>();
       return std::static_pointer_cast<SegmentStreamInterceptor>(interceptor);
    }

protected:
    void ProcessRequestStream(const std::shared_ptr<HttpRequest> &request,
                           const std::shared_ptr<HttpResponse> &response,
                           const ov::String &app_name,
                           const ov::String &stream_name,
                           const ov::String &file_name,
                           const ov::String &file_ext) override;

};
