//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "config/config.h"
#include "base/common_types.h"
#include "base/publisher/publisher.h"
#include "base/media_route/media_route_application_interface.h"
#include "hls_stream_server.h"
#include "hls_stream.h"

//====================================================================================================
// HlsPublisher
//====================================================================================================
class HlsPublisher : public Publisher, public SegmentStreamObserver
{
public:
    static std::shared_ptr<HlsPublisher> Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
                                                 const info::Application *application_info,
                                                 std::shared_ptr<MediaRouteInterface> router);

    HlsPublisher(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

    ~HlsPublisher();

public :
    bool GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections) override;

private :
    bool Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager);

    bool SupportedCodecCheck();

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
    cfg::PublisherType GetPublisherType() override
    {
        return cfg::PublisherType::Hls;
    }

private :

    std::shared_ptr<HlsStreamServer> _stream_server = nullptr;
    bool _supported_codec_check = false;
};
