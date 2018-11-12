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

// 미디어 라우터 구조체
#include "base/media_route/media_route_interface.h"
#include "base/media_route/media_buffer.h"

// 공옹 구조체
#include "base/application/stream_info.h"

#include "transcode_application.h"

#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>

class Transcoder
{
	// class TranscodeApplication;
public:
	static std::shared_ptr<Transcoder> Create(const std::vector<info::Application> &application_list, std::shared_ptr<MediaRouteInterface> router);

	Transcoder(const std::vector<info::Application> &application_list, std::shared_ptr<MediaRouteInterface> router);
	~Transcoder() = default;

	bool Start();
	bool Stop();

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 트랜스코드 어플리케이션 관련 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
public:
	bool CreateApplications();
	bool DeleteApplication();

	// Application Name으로 RouteApplication을 찾음
	std::shared_ptr<TranscodeApplication> GetApplicationById(info::application_id_t application_id);

private:
	std::vector<info::Application> _app_info_list;

	std::map<info::application_id_t, std::shared_ptr<TranscodeApplication>> _tracode_apps;

	std::shared_ptr<MediaRouteInterface> _router;
};

