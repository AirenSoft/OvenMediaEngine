//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Hanb
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
	media_router->Start();
	return media_router;
}

MediaRouter::MediaRouter() 
{
	// logtd("+ MediaRouter");
}

MediaRouter::~MediaRouter()
{
	// logtd("- MediaRouter");
}

bool MediaRouter::Start()
{
	logti("Started media route modules.");

	// 데이터베이스(or Config)에서 어플리케이션 정보를 생성함.
	if(!CraeteApplications())
	{
		return false;
	}

	try 
	{
        _kill_flag = false;
        _thread = std::thread(&MediaRouter::MainTask, this);
    } 
    catch (const std::system_error& e) 
    {
        _kill_flag = true;
		logte("Failed to start media router thread.");
        return false;
    }

  
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
// TODO: Global Config에서 설정값을 읽어옴
bool MediaRouter::CraeteApplications()
{
	auto application_infos = ConfigManager::Instance()->GetApplicationInfos();
	for(auto const& application_info : application_infos)
	{
		//TODO(soulk) : 어플리케이션 구분 코드를 Name에서 Id 체계로 변경해야함.
		ov::String key_app = application_info->GetName();

		auto route_app = MediaRouteApplication::Create(application_info);
		if(route_app == nullptr)
		{
			logte("failed to allocation");
			continue;
		}
		
		// 라우터 어플리케이션 관리 항목에 추가
		_route_apps.insert ( 
			std::make_pair(key_app.CStr(), route_app)
		);
	}

	return true;
}

// 어플리케이션의 스트림이 삭제됨
bool MediaRouter::DeleteApplication()
{
	for(auto const& _route_app : _route_apps)
	{
		_route_app.second->Stop();
	}

	_route_apps.clear();

	return true;
}

//  Application Name으로 RouteApplication을 찾음
std::shared_ptr<MediaRouteApplication> MediaRouter::GetRouteApplicationByName(std::string app_name) 
{ 
	auto obj = _route_apps.find(app_name);

	if(obj == _route_apps.end())
	{
		return NULL;
	}

	return  obj->second;
}

// Connector의 Application이 생성되면 라우터에 등록함
bool MediaRouter::RegisterConnectorApp(
	std::shared_ptr<ApplicationInfo> app_info,
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	// 커넥터의 어플리케이션 명을 받음.
	ov::String app_name = app_info->GetName();
	if(app_name.IsEmpty())
	{
		return true;
	}

	// logtd("Register connector application. app_ptr(%p) name(%s)", app_conn.get(), app_name.CStr());

	// 1. 미디어 라우터 포인터 설정
	// app_conn->SetMediaRouter(this->GetSharedPtr());

	// 2. Media Route Application 모듈에 Connector를 등록함
	auto media_route_app = GetRouteApplicationByName(app_name.CStr()); 

	if(media_route_app == NULL)
	{
		return false;
	}

	return media_route_app->RegisterConnectorApp(app_conn);
}

// 어플리케이션의 스트림이 생성됨
bool MediaRouter::UnregisterConnectorApp(
	std::shared_ptr<ApplicationInfo> app_info,
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	// 커넥터의 어플리케이션 명을 받음.
	ov::String app_name = app_info->GetName();

	if(app_name.IsEmpty())
	{
		return true;
	}

	// logtd("Unregistred connector application. app_ptr(%p) name(%s)", app_conn.get(), app_name.CStr());

	auto media_route_app = GetRouteApplicationByName(app_name.CStr()); 
	if(media_route_app == NULL)
	{
		return false;
	}

	return media_route_app->UnregisterConnectorApp(app_conn);
}

bool MediaRouter::RegisterObserverApp(
	std::shared_ptr<ApplicationInfo> app_info, std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{ 
	if(app_info == NULL || app_obsrv == NULL)
	{
		return false;
	}
	
	// 어플리케이션 명 조회..
	ov::String app_name = app_info->GetName();
	if(app_name.IsEmpty())
	{
		logtw("invalid application name");
		return false;
	}

	// logtd("Register observer application. app_ptr(%p) name(%s)", app_obsrv.get(), app_name.CStr());


	// 2. Media Route Application 모듈에 Connector를 등록함
	auto media_route_app = GetRouteApplicationByName(app_name.CStr()); 
	if(media_route_app == NULL)
	{
		logtw("can not find application. name(%s)", app_name.CStr());
		return false;
	}

	return media_route_app->RegisterObserverApp(app_obsrv);
}

bool MediaRouter::UnregisterObserverApp(
	std::shared_ptr<ApplicationInfo> app_info, std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{ 
	if(app_info == NULL || app_obsrv == NULL)
	{
		return false;
	}

	// 어플리케이션 명 조회.
	ov::String app_name = app_info->GetName();
	if(app_name.IsEmpty())
	{
		logtw(" invalid application name");
		return false;
	}

	// logtd("- Unregistred observer applicaiton. app_ptr(%p) name(%s)", app_obsrv.get(), app_name.CStr());

	auto media_route_app = GetRouteApplicationByName(app_name.CStr()); 
	if(media_route_app == NULL)
	{
		logtw("can not find application. name(%s)", app_name.CStr());
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
		for (auto it=_route_apps.begin(); it!=_route_apps.end(); ++it)
		{
	    	auto application = it->second;
	    
	    	application->OnGarbageCollector();
	    }
    }

}
