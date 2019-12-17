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

// Media Router base class
#include "base/media_route/media_route_observer.h"
#include "base/media_route/media_route_application_observer.h"
#include "base/media_route/media_route_application_connector.h"
#include "base/media_route/media_route_interface.h"
#include "base/media_route/media_buffer.h"

#include "base/info/stream_info.h"
#include "media_route_application.h"

#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>
#include <orchestrator/orchestrator.h>

class MediaRouter : public MediaRouteInterface, public OrchestratorMediaRouterModuleInterface
{
public:
	static std::shared_ptr<MediaRouter> Create();

	MediaRouter();
	~MediaRouter() final;

	bool Start();
	bool Stop();

	////////////////////////////////////////////////////////////////////////////////////////////////
	// For Applications
	////////////////////////////////////////////////////////////////////////////////////////////////
	//  Application Name으로 RouteApplication을 찾음
	std::shared_ptr<MediaRouteApplication> GetRouteApplicationById(info::application_id_t application_id);
	bool CreateApplication(const info::Application &app_info);
	bool DeleteApplication(const info::Application &app_info);

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Implement MediaRouterInterface
	////////////////////////////////////////////////////////////////////////////////////////////////

	bool RegisterModule(const std::shared_ptr<MediaRouteObserver> &module) override;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// For Providers
	////////////////////////////////////////////////////////////////////////////////////////////////

	bool RegisterConnectorApp(
		const info::Application &application_info,
		const std::shared_ptr<MediaRouteApplicationConnector> &application_connector) override;

	bool UnregisterConnectorApp(
		const info::Application &application_info,
		const std::shared_ptr<MediaRouteApplicationConnector> &application_connector) override;


	////////////////////////////////////////////////////////////////////////////////////////////////
	// For Publishers
	////////////////////////////////////////////////////////////////////////////////////////////////
	bool RegisterObserverApp(
		const info::Application &application_info,
		const std::shared_ptr<MediaRouteApplicationObserver> &application_observer) override;

	bool UnregisterObserverApp(
		const info::Application &application_info,
		const std::shared_ptr<MediaRouteApplicationObserver> &application_observer) override;

private:
	void MainTask();

	std::map<info::application_id_t, std::shared_ptr<MediaRouteApplication>> _route_apps;
	std::vector<std::shared_ptr<MediaRouteObserver>> _modules;

	volatile bool _kill_flag;
	std::thread _thread;
	std::mutex _mutex;

};

