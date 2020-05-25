//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <iostream>
#include <unistd.h>

#include "media_router.h"
#include "config/config_manager.h"

#define OV_LOG_TAG "MediaRouter"

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
	logti("MediaRouter has been started.");

	return true;
}

bool MediaRouter::Stop()
{
	logti("MediaRouter has been stopped.");

	for(auto const &_route_app : _route_apps)
	{
		_route_app.second->Stop();
	}

	_route_apps.clear();

	return true;
}

bool MediaRouter::OnCreateApplication(const info::Application &app_info)
{
	info::application_id_t application_id = app_info.GetId();
	auto route_app = MediaRouteApplication::Create(app_info);
	if(route_app == nullptr)
	{
		logte("failed to allocation route_app");
		return false;
	}
	else
	{
		logti("MediaRouter has created [%s] application", app_info.GetName().CStr());
	}
	

	_route_apps.insert(std::make_pair(application_id, route_app));

	return true;
}

bool MediaRouter::OnDeleteApplication(const info::Application &app_info)
{
	info::application_id_t application_id = app_info.GetId();

	// Remove from the Route App Map
	_route_apps[application_id]->Stop();
	
	_route_apps.erase(application_id);

	logti("MediaRouter has deleted [%s] application", app_info.GetName().CStr());

	return true;
}

//  Application Name으로 RouteApplication을 찾음
std::shared_ptr<MediaRouteApplication> MediaRouter::GetRouteApplicationById(info::application_id_t application_id)
{
	auto obj = _route_apps.find(application_id);

	if(obj == _route_apps.end())
	{
		return nullptr;
	}

	return obj->second;
}

// Connector의 Application이 생성되면 라우터에 등록함
bool MediaRouter::RegisterConnectorApp(
	const info::Application &application_info,
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn)
{
	// logtd("Register connector application. app_ptr(%p) name(%s)", app_conn.get(), app_name.CStr());

	// 1. 미디어 라우터 포인터 설정
	// app_conn->SetMediaRouter(this->GetSharedPtr());

	// 2. Media Route Application 모듈에 Connector를 등록함
	auto media_route_app = GetRouteApplicationById(application_info.GetId());
	if(media_route_app == nullptr)
	{
		return false;
	}

	return media_route_app->RegisterConnectorApp(app_conn);
}

// Application stream created
bool MediaRouter::UnregisterConnectorApp(
	const info::Application &application_info,
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn)
{
	// logtd("Unregistred connector application. app_ptr(%p) name(%s)", app_conn.get(), app_name.CStr());

	auto media_route_app = GetRouteApplicationById(application_info.GetId());
	if(media_route_app == nullptr)
	{
		return false;
	}

	return media_route_app->UnregisterConnectorApp(app_conn);
}

bool MediaRouter::RegisterObserverApp(
	const info::Application &application_info, const std::shared_ptr<MediaRouteApplicationObserver> &app_obsrv)
{
	if(app_obsrv == nullptr)
	{
		return false;
	}

	// 2. Media Route Application 모듈에 Connector를 등록함
	auto media_route_app = GetRouteApplicationById(application_info.GetId());
	if(media_route_app == nullptr)
	{
		logtw("cannot find application. name(%s)", application_info.GetName().CStr());
		return false;
	}

	return media_route_app->RegisterObserverApp(app_obsrv);
}

bool MediaRouter::UnregisterObserverApp(
	const info::Application &application_info, const std::shared_ptr<MediaRouteApplicationObserver> &app_obsrv)
{
	if(app_obsrv == nullptr)
	{
		return false;
	}

	// logtd("- Unregistred observer applicaiton. app_ptr(%p) name(%s)", app_obsrv.get(), app_name.CStr());

	auto media_route_app = GetRouteApplicationById(application_info.GetId());
	if(media_route_app == nullptr)
	{
		logtw("cannot find application. name(%s)", application_info.GetName().CStr());
		return false;
	}

	return media_route_app->UnregisterObserverApp(app_obsrv);
}

