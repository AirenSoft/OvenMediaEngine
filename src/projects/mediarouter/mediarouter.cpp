//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mediarouter.h"

#include <unistd.h>

#include <iostream>

#include "config/config_manager.h"
#include "mediarouter_private.h"

std::shared_ptr<MediaRouter> MediaRouter::Create()
{
	auto media_router = std::make_shared<MediaRouter>();
	if (!media_router->Start())
	{
		logte("An error occurred while creating MediaRouter");
		return nullptr;
	}
	return media_router;
}

MediaRouter::MediaRouter()
{
}

MediaRouter::~MediaRouter()
{
}

bool MediaRouter::Start()
{
	logti("Mediarouter has been started.");

	return true;
}

bool MediaRouter::Stop()
{
	logti("Mediarouter has been stopped.");

	for (auto const &_route_app : _route_apps)
	{
		_route_app.second->Stop();
	}

	_route_apps.clear();

	return true;
}

// Application created by the orchestrator
bool MediaRouter::OnCreateApplication(const info::Application &app_info)
{
	info::application_id_t application_id = app_info.GetId();
	auto route_app = MediaRouteApplication::Create(app_info);
	if (route_app == nullptr)
	{
		logte("Failed to memory allcation. app(%s)", app_info.GetName().CStr());
		return false;
	}
	else
	{
		logti("Created Mediarouter. app(%s)", app_info.GetName().CStr());
	}

	_route_apps.insert(std::make_pair(application_id, route_app));

	return true;
}

// Application deleted by the orchestrator
bool MediaRouter::OnDeleteApplication(const info::Application &app_info)
{
	info::application_id_t application_id = app_info.GetId();

	// Remove from the Route App Map
	_route_apps[application_id]->Stop();

	_route_apps.erase(application_id);

	logti("Mediarouter has been destroyed. app(%s)", app_info.GetName().CStr());

	return true;
}

//  Search by application id
std::shared_ptr<MediaRouteApplication> MediaRouter::GetRouteApplicationById(info::application_id_t application_id)
{
	auto obj = _route_apps.find(application_id);

	if (obj == _route_apps.end())
	{
		return nullptr;
	}

	return obj->second;
}

// Registers the requested Connector application
bool MediaRouter::RegisterConnectorApp(
	const info::Application &application_info,
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn)
{
	auto media_route_app = GetRouteApplicationById(application_info.GetId());
	if (media_route_app == nullptr)
	{
		return false;
	}

	return media_route_app->RegisterConnectorApp(app_conn);
}

// Unregisters the requested Connector application
bool MediaRouter::UnregisterConnectorApp(
	const info::Application &application_info,
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn)
{
	auto media_route_app = GetRouteApplicationById(application_info.GetId());
	if (media_route_app == nullptr)
	{
		return false;
	}

	return media_route_app->UnregisterConnectorApp(app_conn);
}

// Register requested Observer application
bool MediaRouter::RegisterObserverApp(
	const info::Application &application_info, const std::shared_ptr<MediaRouteApplicationObserver> &app_obsrv)
{
	if (app_obsrv == nullptr)
	{
		return false;
	}

	auto media_route_app = GetRouteApplicationById(application_info.GetId());
	if (media_route_app == nullptr)
	{
		logtw("Could not found application. app(%s)", application_info.GetName().CStr());
		return false;
	}

	return media_route_app->RegisterObserverApp(app_obsrv);
}

// Unregister requested Observer application
bool MediaRouter::UnregisterObserverApp(
	const info::Application &application_info, const std::shared_ptr<MediaRouteApplicationObserver> &app_obsrv)
{
	if (app_obsrv == nullptr)
	{
		return false;
	}

	// logtd("- Unregistred observer applicaiton. app_ptr(%p) name(%s)", app_obsrv.get(), app_name.CStr());

	auto media_route_app = GetRouteApplicationById(application_info.GetId());
	if (media_route_app == nullptr)
	{
		logtw("Could not found application. app(%s)", application_info.GetName().CStr());
		return false;
	}

	return media_route_app->UnregisterObserverApp(app_obsrv);
}
