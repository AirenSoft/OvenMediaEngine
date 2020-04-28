//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <stdint.h>
#include <unistd.h>
#include <algorithm>
#include <memory>
#include <thread>
#include <vector>

#include <base/ovlibrary/ovlibrary.h>
#include <orchestrator/orchestrator.h>

#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"
#include "base/provider/application.h"
#include "base/provider/provider.h"

#include "rtmp_observer.h"
#include "rtmp_server.h"

class RtmpProvider : public pvd::Provider, public RtmpObserver
{
public:
	static std::shared_ptr<RtmpProvider> Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);

	explicit RtmpProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
	~RtmpProvider() override;

	ProviderStreamDirection GetProviderStreamDirection() const override
	{
		return ProviderStreamDirection::Push;
	}

	ProviderType GetProviderType() const override
	{
		return ProviderType::Rtmp;
	}

	const char* GetProviderName() const override
	{
		return "RtmpProvider";
	}

	bool Start() override;
	bool Stop() override;

protected:
	std::shared_ptr<pvd::Application> OnCreateProviderApplication(const info::Application &application_info) override;
	bool OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application) override;

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
					 int64_t timestamp,
					 RtmpFrameType frame_type,
					 const std::shared_ptr<const ov::Data> &data) override;

	bool OnAudioData(info::application_id_t application_id,
					 uint32_t stream_id,
					 int64_t timestamp,
					 RtmpFrameType frame_type,
					 const std::shared_ptr<const ov::Data> &data) override;

	bool OnDeleteStream(info::application_id_t application_id, uint32_t stream_id) override;

private:
	std::shared_ptr<RtmpServer> _rtmp_server;
};
