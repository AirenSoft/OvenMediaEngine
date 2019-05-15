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
#include "dash_stream_server.h"
#include "hls_stream_server.h"

//====================================================================================================
// SegmentStreamPublisher
//====================================================================================================
class SegmentStreamPublisher : public Publisher, public SegmentStreamObserver
{
public:
    static std::shared_ptr<SegmentStreamPublisher> Create(cfg::PublisherType publisher_type,
                                                    std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
                                                    const info::Application *application_info,
                                                    std::shared_ptr<MediaRouteInterface> router);

    SegmentStreamPublisher(cfg::PublisherType publisher_type,
                            const info::Application *application_info,
                            std::shared_ptr<MediaRouteInterface> router);

    ~SegmentStreamPublisher();

public :
    bool GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections) override;

private :
    bool Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager);

    void DashStart(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
                   const cfg::Port &port,
                   std::shared_ptr<Certificate> certificate,
                   std::shared_ptr<Certificate> chain_certificate,
                   const cfg::DashPublisher *publisher_info);

    void HlsStart(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
                  const cfg::Port &port,
                  std::shared_ptr<Certificate> certificate,
                  std::shared_ptr<Certificate> chain_certificate,
                  const cfg::HlsPublisher *publisher_info);

    bool Stop() override;

    std::shared_ptr<Application> OnCreateApplication(const info::Application *application_info) override;

    // SegmentStreamObserver Implementation
    bool OnPlayListRequest(const ov::String &app_name,
                           const ov::String &stream_name,
                           const ov::String &file_name,
                           ov::String &play_list) override;

    bool OnSegmentRequest(const ov::String &app_name,
                          const ov::String &stream_name,
                          const ov::String &file_name,
                          std::shared_ptr<ov::Data> &segment_data) override;

    // Publisher Implementation
    cfg::PublisherType GetPublisherType() override { return _publisher_type; }

private :
    cfg::PublisherType _publisher_type;
    std::shared_ptr<SegmentStreamServer> _segment_stream_server = nullptr;
};
