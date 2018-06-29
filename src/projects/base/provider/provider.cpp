//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Hanb
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================


#include "provider.h"

#include "application.h"
#include "stream.h"

#define OV_LOG_TAG "Provider"

namespace pvd
{
	Provider::Provider(std::shared_ptr<MediaRouteInterface> router)
	{
		_router = router;
	}
	Provider::~Provider()
	{

	}

	bool Provider::Start()
	{
		// TODO: CONFIG 에서 Application 설정을 읽어와서 생성해야 한다.
		// config->application->publish->get(GetPublishName()) 로 수정
		if(GetProviderName() == "RTMP") //
		{
			ApplicationInfo app_info;
			// Config 에서 Application 정보를 읽어온다.
			app_info.SetName(ov::String::FormatString("app"));
			// Application 을 자식에게 생성하게 한다.
			auto application = OnCreateApplication(app_info);
			// 생성한 Application을 Router와 연결하고 Start
			_router->RegisterConnectorApp(application, application);
			// Apllication Map에 보관
			_applications[application->GetId()] = application;
			// 시작한다.
			application->Start();
		}

		return true;
	}

	bool Provider::Stop()
	{
		auto it = _applications.begin();
		while( it != _applications.end())
		{
			auto application = it->second;
			
			_router->UnregisterConnectorApp(application, application);
			application->Stop();
			
			it = _applications.erase(it);
		}

		return true;
	}

	std::shared_ptr<Application> Provider::GetApplication(ov::String app_name)
	{
		for(auto const & x : _applications)
		{
			auto application = x.second;
			if(application->GetName() == app_name)
			{
				return application;
			}
		}

		return nullptr;
	}

	std::shared_ptr<Stream>	Provider::GetStream(ov::String app_name, ov::String stream_name)
	{
		auto app = GetApplication(app_name);
		if(!app)
		{
			return nullptr;
		}

		return app->GetStream(stream_name);
	}

	std::shared_ptr<Application> Provider::GetApplication(uint32_t app_id)
	{
		if(_applications.count(app_id) <= 0)
		{
			return nullptr;
		}

		return _applications[app_id];
	}

	std::shared_ptr<Stream>	Provider::GetStream(uint32_t app_id, uint32_t stream_id)
	{
		auto app = GetApplication(app_id);
		if(!app)
		{
			return nullptr;
		}

		return app->GetStream(stream_id);
	}
}