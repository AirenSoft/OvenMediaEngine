//==============================================================================
//
//  Transcoder
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <stdint.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>
#include "base/mediarouter/media_route_interface.h"
#include "base/mediarouter/media_buffer.h"
#include "base/info/stream.h"
#include "transcode_application.h"

#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>
#include <orchestrator/orchestrator.h>


class Transcoder : public ocst::TranscoderModuleInterface
{
	// class TranscodeApplication;
public:
	static std::shared_ptr<Transcoder> Create(std::shared_ptr<MediaRouteInterface> router);

	Transcoder(std::shared_ptr<MediaRouteInterface> router);
	~Transcoder() = default;

	bool Start();
	bool Stop();

	//--------------------------------------------------------------------
	// Implementation of ModuleInterface
	//--------------------------------------------------------------------
	bool OnCreateApplication(const info::Application &app_info) override;
	bool OnDeleteApplication(const info::Application &app_info) override;

private:
	// Application Name으로 RouteApplication을 찾음
	std::shared_ptr<TranscodeApplication> GetApplicationById(info::application_id_t application_id);


	std::vector<info::Application> _app_info_list;
	std::map<info::application_id_t, std::shared_ptr<TranscodeApplication>> _tracode_apps;
	std::shared_ptr<MediaRouteInterface> _router;
};

