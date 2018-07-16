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
#include "base/application/stream_info.h"
#include "base/application/application_info.h"
#include "media_route_application.h"

#include <base/ovlibrary/ovlibrary.h>


// -어플리케이션(Application) 별 스트림(Stream)을 관리해야 한다
// - Publisher를 관리해야한다
// - Provider를 관리해야한다
class MediaRouter : public MediaRouteInterface
{
public:
	static std::shared_ptr<MediaRouter> Create();

    MediaRouter();
    ~MediaRouter();

    bool Start();
    bool Stop();

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 라우트 어플리케이션 관련 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
public:
	bool CraeteApplications();
	bool DeleteApplication();

	//  Application Name으로 RouteApplication을 찾음
	std::shared_ptr<MediaRouteApplication> GetRouteApplicationByName(std::string app_name);

private:
	std::map<std::string, std::shared_ptr<MediaRouteApplication>> _route_apps;


	////////////////////////////////////////////////////////////////////////////////////////////////
	// 프로바이더(Provider) 관련 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
	// 본 데이터 모듈에 넣어주는 놈들
public:
   	bool RegisterConnectorApp(
   		std::shared_ptr<ApplicationInfo> app_info,
   		std::shared_ptr<MediaRouteApplicationConnector> application_connector);

	bool UnregisterConnectorApp(
		std::shared_ptr<ApplicationInfo> app_info,
		std::shared_ptr<MediaRouteApplicationConnector> application_connector);


private:
	////////////////////////////////////////////////////////////////////////////////////////////////
	// 퍼블리셔 관련 모듈
	////////////////////////////////////////////////////////////////////////////////////////////////
	// 본 데이터 모듈에서 가져가는 넘들
public:
	bool RegisterObserverApp(
		std::shared_ptr<ApplicationInfo> app_info, 
		std::shared_ptr<MediaRouteApplicationObserver> application_observer);

	bool UnregisterObserverApp(
		std::shared_ptr<ApplicationInfo> app_info, 
		std::shared_ptr<MediaRouteApplicationObserver> application_observer);

private:
	void MainTask();
	volatile bool 	_kill_flag;
    std::thread 	_thread;
    std::mutex 		_mutex;

};

