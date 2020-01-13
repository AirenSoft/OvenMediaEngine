//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================


#include <zconf.h>
#include "provider.h"

#include "application.h"
#include "stream.h"

#define OV_LOG_TAG "Provider"

namespace pvd
{
	Provider::Provider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: _server_config(server_config), _router(router)
	{
		_use_garbage_collector = false;
	}

	Provider::~Provider()
	{
	}

	const cfg::Server &Provider::GetServerConfig() const
	{
		return _server_config;
	}

	bool Provider::Start()
	{
		if(_use_garbage_collector)
		{
			_worker_thread = std::thread(&Provider::GarbageCollector, this);
			_worker_thread.detach();
		}

		return true;
	}

	bool Provider::SetUseAutoStreamRemover(bool use)
	{
		_use_garbage_collector = use;
	}

	bool Provider::Stop()
	{
		_use_garbage_collector = false;

		auto it = _applications.begin();

		while(it != _applications.end())
		{
			auto application = it->second;

			_router->UnregisterConnectorApp(*application.get(), application);
			application->Stop();

			it = _applications.erase(it);
		}

		return true;
	}

	// Create Application
	bool Provider::OnCreateApplication(const info::Application &app_info)
	{
		if(_router == nullptr)
		{
			logte("Could not find MediaRouter");
			OV_ASSERT2(false);
			return false;
		}

		// Let child create application
		auto application = OnCreateProviderApplication(app_info);

		if(application == nullptr)
		{
			logte("Could not create application for [%s]", app_info.GetName().CStr());
			return false;
		}

		// Connect created application to router
		if(_router->RegisterConnectorApp(*application.get(), application) == false)
		{
			logte("Could not register the application: %p", application.get());
			return false;
		}

		// Store created application
		_applications[application->GetId()] = application;

		application->Start();

		return true;
	}

	// Delete Application
	bool Provider::OnDeleteApplication(const info::Application &app_info)
	{
		auto item = _applications.find(app_info.GetId());

		logti("Deleting the application: [%s]", app_info.GetName().CStr());

		if(item == _applications.end())
		{
			logte("The application does not exists: [%s]", app_info.GetName().CStr());
			return false;
		}

		bool result = OnDeleteProviderApplication(app_info);

		if(result == false)
		{
			logte("Could not delete the application: [%s]", app_info.GetName().CStr());
			return false;
		}

		_applications[app_info.GetId()]->Stop();
		_applications.erase(app_info.GetId());

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

	void Provider::GarbageCollector()
	{
		while(_use_garbage_collector)
		{
			for(auto const &x : _applications)
			{
				auto app = x.second;
				app->DeleteTerminatedStreams();
			}

			sleep(1);
		}
	}
}