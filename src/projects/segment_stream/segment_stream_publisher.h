//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include "base/common_types.h"
#include "base/publisher/publisher.h"
#include "base/media_route/media_route_application_interface.h"
#include "segment_stream_server.h"

//====================================================================================================
// SegmentStreamPublisher
//====================================================================================================
class SegmentStreamPublisher : public Publisher, public SegmentStreamObserver
{
public:
    static std::shared_ptr<SegmentStreamPublisher> Create(const info::Application &application_info, std::shared_ptr<MediaRouteInterface> router);

	SegmentStreamPublisher(const info::Application &application_info, std::shared_ptr<MediaRouteInterface> router);
	~SegmentStreamPublisher();

public :

private :
    uint16_t     GetSegmentStreamPort();
    bool        Start() override;
    bool        Stop() override ;
    std::shared_ptr<Application>    OnCreateApplication(const info::Application &application_info) override;

    // SegmentStreamObserver Implementation
    bool        OnPlayListRequest(const ov::String &app_name, const ov::String &stream_name, PlayListType play_list_type, ov::String &play_list) override;
    bool        OnSegmentRequest(const ov::String &app_name, const ov::String &stream_name, SegmentType segment_type, const ov::String &file_name, std::shared_ptr<ov::Data> &segment_data) override;
    bool        OnCrossdomainRequest(ov::String &cross_domain) override;
    bool        OnCorsCheck(const ov::String &app_name, const ov::String &stream_name, ov::String &origin_url) override;
private :
    // Publisher Implementation
    cfg::PublisherType GetPublisherType() override
    {
        return cfg::PublisherType::Hls; // 임시( 차후 hls/dash 통합 처리)
    }

    std::shared_ptr<SegmentStreamServer> _segment_stream_server;
};
