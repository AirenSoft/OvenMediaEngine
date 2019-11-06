//==============================================================================
//
//  MediaRouter
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
#include "base/media_route/media_route_application_observer.h"
#include "base/media_route/media_route_application_connector.h"
#include "base/media_route/media_route_interface.h"
#include "base/media_route/media_buffer.h"

// 공옹 구조체
#include "base/info/stream_info.h"
#include "media_route_application.h"

#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>

// -어플리케이션(Application) 별 스트림(Stream)을 관리해야 한다
// - Publisher를 관리해야한다
// - Provider를 관리해야한다
class MediaRouter : public MediaRouteInterface
{
public:
	static std::shared_ptr<MediaRouter> Create();
	static std::shared_ptr<MediaRouter> Create(const std::vector<info::Application> &app_info_list);

	MediaRouter();
	MediaRouter(const std::vector<info::Application> &app_info_list);
	~MediaRouter();

	bool Start();
	bool Stop();

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 라우트 어플리케이션 관련 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
public:
	//  Application Name으로 RouteApplication을 찾음
	std::shared_ptr<MediaRouteApplication> GetRouteApplicationById(info::application_id_t application_id);
	bool CreateApplication(info::Application app_info);
	bool DeleteApplication(info::Application app_info);

private:
	bool CreateApplications();
	bool DeleteApplications();

	std::map<info::application_id_t, std::shared_ptr<MediaRouteApplication>> _route_apps;


	////////////////////////////////////////////////////////////////////////////////////////////////
	// 프로바이더(Provider) 관련 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
	// MediaRouter에 데이터를 전송하는 모듈 등록/해제
public:
	bool RegisterConnectorApp(
		const info::Application *application_info,
		std::shared_ptr<MediaRouteApplicationConnector> application_connector) override;

	bool UnregisterConnectorApp(
		const info::Application *application_info,
		std::shared_ptr<MediaRouteApplicationConnector> application_connector) override;


	////////////////////////////////////////////////////////////////////////////////////////////////
	// 퍼블리셔 관련 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
	// MediaRouter가 데이터를 보내는 모듈 등록/해제
	bool RegisterObserverApp(
		const info::Application *application_info,
		std::shared_ptr<MediaRouteApplicationObserver> application_observer) override;

	bool UnregisterObserverApp(
		const info::Application *application_info,
		std::shared_ptr<MediaRouteApplicationObserver> application_observer) override;

private:
	void MainTask();

	std::vector<info::Application> _app_info_list;

	volatile bool _kill_flag;
	std::thread _thread;
	std::mutex _mutex;

};

