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

std::shared_ptr<MediaRouter> MediaRouter::Create(const std::vector<info::Application> &app_info_list)
{
	auto media_router = std::make_shared<MediaRouter>(app_info_list);
	media_router->Start();
	return media_router;
}

MediaRouter::MediaRouter(const std::vector<info::Application> &app_info_list)
{
	_app_info_list = app_info_list;

	// logtd("+ MediaRouter");
}

MediaRouter::~MediaRouter()
{
	// logtd("- MediaRouter");
}

bool MediaRouter::Start()
{
	logti("Trying to start media router...");

	// Create applications using information in the config
	if(CreateApplications() == false)
	{
		return false;
	}

	try
	{
		_kill_flag = false;
		_thread = std::thread(&MediaRouter::MainTask, this);
	}
	catch(const std::system_error &e)
	{
		_kill_flag = true;

		logte("Failed to start media router thread.");

		return false;
	}

	logti("Media router is started.");

	return true;
}

bool MediaRouter::Stop()
{
	logti("Terminated media route modules.");

	_kill_flag = true;
	_thread.join();

	if(!DeleteApplication())
	{
		return false;
	}

	// TODO: 패킷 처리 스레드를 만들어야함.. 어플리케이션 단위로 만들어 버릴까?
	return true;
}

// 어플리케이션의 스트림이 생성됨
bool MediaRouter::CreateApplications()
{
	for(auto const &application_info : _app_info_list)
	{
		info::application_id_t application_id = application_info.GetId();

		auto route_app = MediaRouteApplication::Create(&application_info);

		if(route_app == nullptr)
		{
			logte("failed to allocation");
			continue;
		}

		// 라우터 어플리케이션 관리 항목에 추가
		_route_apps.insert(
			std::make_pair(application_id, route_app)
		);
	}

	return true;
}

// 어플리케이션의 스트림이 삭제됨
bool MediaRouter::DeleteApplication()
{
	for(auto const &_route_app : _route_apps)
	{
		_route_app.second->Stop();
	}

	_route_apps.clear();

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
	const info::Application *application_info,
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	// logtd("Register connector application. app_ptr(%p) name(%s)", app_conn.get(), app_name.CStr());

	// 1. 미디어 라우터 포인터 설정
	// app_conn->SetMediaRouter(this->GetSharedPtr());

	// 2. Media Route Application 모듈에 Connector를 등록함
	auto media_route_app = GetRouteApplicationById(application_info->GetId());

	if(media_route_app == nullptr)
	{
		return false;
	}

	return media_route_app->RegisterConnectorApp(app_conn);
}

// 어플리케이션의 스트림이 생성됨
bool MediaRouter::UnregisterConnectorApp(
	const info::Application *application_info,
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	// logtd("Unregistred connector application. app_ptr(%p) name(%s)", app_conn.get(), app_name.CStr());

	auto media_route_app = GetRouteApplicationById(application_info->GetId());
	if(media_route_app == nullptr)
	{
		return false;
	}

	return media_route_app->UnregisterConnectorApp(app_conn);
}

bool MediaRouter::RegisterObserverApp(
	const info::Application *application_info, std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	if(app_obsrv == nullptr)
	{
		return false;
	}

	// logtd("Register observer application. app_ptr(%p) name(%s)", app_obsrv.get(), app_name.CStr());


	// 2. Media Route Application 모듈에 Connector를 등록함
	auto media_route_app = GetRouteApplicationById(application_info->GetId());

	if(media_route_app == nullptr)
	{
		logtw("cannot find application. name(%s)", application_info->GetName().CStr());
		return false;
	}

	return media_route_app->RegisterObserverApp(app_obsrv);
}

bool MediaRouter::UnregisterObserverApp(
	const info::Application *application_info, std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	if(app_obsrv == nullptr)
	{
		return false;
	}

	// logtd("- Unregistred observer applicaiton. app_ptr(%p) name(%s)", app_obsrv.get(), app_name.CStr());

	auto media_route_app = GetRouteApplicationById(application_info->GetId());
	if(media_route_app == nullptr)
	{
		logtw("cannot find application. name(%s)", application_info->GetName().CStr());
		return false;
	}

	return media_route_app->UnregisterObserverApp(app_obsrv);
}


// Application -> Stream -> BufferQueue에 있는 Buffer를 Observer에 전달하는 목적으로 사용됨.
void MediaRouter::MainTask()
{
	while(!_kill_flag)
	{
		sleep(5);
		// logtd("Perform a garbage collector");
		for(auto it = _route_apps.begin(); it != _route_apps.end(); ++it)
		{
			auto application = it->second;

			application->OnGarbageCollector();
		}
	}

}
