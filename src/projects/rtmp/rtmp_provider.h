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
#include "base/application/application.h"

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
	static std::shared_ptr<RtmpProvider> Create(std::shared_ptr<MediaRouteInterface> router);

     explicit RtmpProvider(std::shared_ptr<MediaRouteInterface> router);
     ~RtmpProvider();

    // 프로파이더의 이름을 구분함
	ov::String 	GetProviderName() override	{ return "RTMP"; }

	bool        Start() override ;
	bool        Stop() override ;

	std::shared_ptr<pvd::Application> OnCreateApplication(ApplicationInfo &info);

	//--------------------------------------------------------------------
	// Implementation of RtmpObserver
	//--------------------------------------------------------------------
    bool        OnStreamReadyComplete(const ov::String &app_name, const ov::String &stream_name, std::shared_ptr<RtmpMediaInfo> &media_info, uint32_t &app_id, uint32_t &stream_id) override;
    bool        OnVideoData(uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data) override;
    bool        OnAudioData(uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data) override;
	bool        OnDeleteStream(uint32_t app_id, uint32_t stream_id) override;

private :
	std::shared_ptr<RtmpServer>	_rtmp_server;
};

