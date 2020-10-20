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
#include "base/mediarouter/media_route_application_observer.h"
#include "base/mediarouter/media_route_application_connector.h"
#include "base/mediarouter/media_route_interface.h"
#include "base/mediarouter/media_buffer.h"

#include "base/info/stream.h"
#include "mediarouter_application.h"
#include "mediarouter_stream.h"

#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>
#include <orchestrator/orchestrator.h>

class MediaRouter : public MediaRouteInterface, public ocst::MediaRouterModuleInterface
{
public:
	static std::shared_ptr<MediaRouter> Create();

	MediaRouter();
	~MediaRouter() final;

	bool Start();
	bool Stop();

	//--------------------------------------------------------------------
	// Implementation of ModuleInterface
	//--------------------------------------------------------------------
	bool OnCreateApplication(const info::Application &app_info) override;
	bool OnDeleteApplication(const info::Application &app_info) override;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// For Applications
	////////////////////////////////////////////////////////////////////////////////////////////////
	//  Application Name으로 RouteApplication을 찾음
	std::shared_ptr<MediaRouteApplication> GetRouteApplicationById(info::application_id_t application_id);

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
	std::map<info::application_id_t, std::shared_ptr<MediaRouteApplication>> _route_apps;

};

