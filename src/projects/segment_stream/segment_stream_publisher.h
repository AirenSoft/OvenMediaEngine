//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "../config/config.h"
#include "../base/common_types.h"
#include "../base/publisher/publisher.h"
#include "../base/media_route/media_route_application_interface.h"
#include "segment_stream_server.h"

//====================================================================================================
// SegmentStreamPublisher
//====================================================================================================
class SegmentStreamPublisher : public Publisher, public SegmentStreamObserver {
public:
    static std::shared_ptr<SegmentStreamPublisher>
    Create(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

    SegmentStreamPublisher(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

    ~SegmentStreamPublisher();

public :
    bool GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections) override;

private :
    bool Start() override;

    bool Stop() override;

    std::shared_ptr<Application> OnCreateApplication(const info::Application *application_info) override;

    // SegmentStreamObserver Implementation
    bool OnPlayListRequest(const ov::String &app_name,
                           const ov::String &stream_name,
                           const ov::String &file_name,
                           PlayListType play_list_type,
                           ov::String &play_list) override;

    bool OnSegmentRequest(const ov::String &app_name,
                          const ov::String &stream_name,
                          SegmentType segment_type,
                          const ov::String &file_name,
                          std::shared_ptr<ov::Data> &segment_data) override;

    // Publisher Implementation
    cfg::PublisherType GetPublisherType() override { return _publisher_type; }
private :
    cfg::PublisherType _publisher_type;
    std::vector<std::shared_ptr<SegmentStreamServer>> _segment_stream_servers;
};
