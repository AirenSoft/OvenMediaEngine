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
#include "dash_packetizer.h"
//====================================================================================================
// DashStreamServer
//====================================================================================================
class DashStreamServer : public SegmentStreamServer
{
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
    void ProcessRequestStream(const std::shared_ptr<HttpResponse> &response,
							   const ov::String &app_name,
							   const ov::String &stream_name,
							   const ov::String &file_name,
							   const ov::String &file_ext) override;

	void OnPlayListRequest(const ov::String &app_name,
						 const ov::String &stream_name,
						 const ov::String &file_name,
						 PlayListType play_list_type,
						 const std::shared_ptr<HttpResponse> &response) override;

	void OnSegmentRequest(const ov::String &app_name,
						const ov::String &stream_name,
						const ov::String &file_name,
						SegmentType segment_type,
						const std::shared_ptr<HttpResponse> &response) override;

};
