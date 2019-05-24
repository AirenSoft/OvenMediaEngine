//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <unistd.h>
#include <stdint.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>

#include <base/ovlibrary/ovlibrary.h>

#include "base/provider/provider.h"
#include "base/provider/application.h"
#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"

#include "rtmp_server.h"
#include "rtmp_observer.h"

class RtmpProvider : public pvd::Provider, public RtmpObserver
{
    // class TranscodeApplication;
public:
    static std::shared_ptr<RtmpProvider>
    Create(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

    explicit RtmpProvider(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

    ~RtmpProvider() override;

    cfg::ProviderType GetProviderType() override
    {
        return cfg::ProviderType::Rtmp;
    }

    bool Start() override;

    bool Stop() override;

    std::shared_ptr<pvd::Application> OnCreateApplication(const info::Application *application_info) override;

    //--------------------------------------------------------------------
    // Implementation of RtmpObserver
    //--------------------------------------------------------------------
    bool OnStreamReadyComplete(const ov::String &app_name,
                               const ov::String &stream_name,
                               std::shared_ptr<RtmpMediaInfo> &media_info,
                               info::application_id_t &application_id,
                               uint32_t &stream_id) override;

    bool OnVideoData(info::application_id_t application_id,
                     uint32_t stream_id,
                     uint32_t timestamp,
                     RtmpFrameType frame_type,
                     std::shared_ptr<std::vector<uint8_t>> &data) override;

    bool OnAudioData(info::application_id_t application_id,
                     uint32_t stream_id,
                     uint32_t timestamp,
                     RtmpFrameType frame_type,
                     std::shared_ptr<std::vector<uint8_t>> &data) override;

    bool OnDeleteStream(info::application_id_t application_id, uint32_t stream_id) override;

private:
    const cfg::RtmpProvider *_provider_info;

    std::shared_ptr<RtmpServer> _rtmp_server;
};

