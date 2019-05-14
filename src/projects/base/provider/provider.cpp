//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================


#include "provider.h"

#include "application.h"
#include "stream.h"

#define OV_LOG_TAG "Provider"

namespace pvd
{
	Provider::Provider(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
		: _application_info(application_info),
		  _router(router)
	{
	}

	Provider::~Provider()
	{
	}

	bool Provider::Start()
	{
		// Application 을 자식에게 생성하게 한다.
		auto application = OnCreateApplication(_application_info);

		// 생성한 Application을 Router와 연결하고 Start
		_router->RegisterConnectorApp(application.get(), application);

		// Apllication Map에 보관
		_applications[application->GetId()] = application;

		// 시작한다.
		application->Start();

		return true;
	}

	bool Provider::Stop()
	{
		auto it = _applications.begin();

		while(it != _applications.end())
		{
			auto application = it->second;

			_router->UnregisterConnectorApp(application.get(), application);

			application->Stop();

			it = _applications.erase(it);
		}

		return true;
	}

	std::shared_ptr<Application> Provider::GetApplicationByName(ov::String app_name)
	{
		for(auto const &x : _applications)
		{
			auto application = x.second;
			if(application->GetName() == app_name)
			{
				return application;
			}
		}

		return nullptr;
	}

	std::shared_ptr<Stream> Provider::GetStreamByName(ov::String app_name, ov::String stream_name)
	{
		auto app = GetApplicationByName(app_name);

		if(!app)
		{
			return nullptr;
		}

		return app->GetStreamByName(stream_name);
	}

	std::shared_ptr<Application> Provider::GetApplicationById(info::application_id_t application_id)
	{
		auto application = _applications.find(application_id);

		if(application != _applications.end())
		{
			return application->second;
		}

		return nullptr;
	}

	std::shared_ptr<Stream> Provider::GetStreamById(info::application_id_t application_id, uint32_t stream_id)
	{
		auto app = GetApplicationById(application_id);

		if(app != nullptr)
		{
			return app->GetStreamById(stream_id);
		}

		return nullptr;
	}
}