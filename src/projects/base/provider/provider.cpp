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
		_run_thread = true;
		_worker_thread = std::thread(&Provider::RegularTask, this);
		_worker_thread.detach();
		
		logti("%s has been started.", GetProviderName());

		return true;
	}

	bool Provider::Stop()
	{
		_run_thread = false;

		auto it = _applications.begin();

		while(it != _applications.end())
		{
			auto application = it->second;

			_router->UnregisterConnectorApp(*application.get(), application);
			application->Stop();

			it = _applications.erase(it);
		}

		logti("%s has been stopped.", GetProviderName());
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

		// Check configuration
		if(app_info.IsDynamicApp() == false)
		{
			auto cfg_provider_list = app_info.GetConfig().GetProviders().GetProviderList();
			for(const auto &cfg_provider : cfg_provider_list)
			{
				if(cfg_provider->GetType() == GetProviderType())
				{
					if(cfg_provider->IsParsed())
					{
						break;
					}
					else
					{
						// This provider is diabled
						logti("%s provider is disabled in %s application, so it was not created", 
								ov::Converter::ToString(GetProviderType()).CStr(), app_info.GetName().CStr());
						return true;
					}
				}
			}
		}
		else
		{
			// The dynamically created app activates all providers
		}

		// Let child create application
		auto application = OnCreateProviderApplication(app_info);
		if(application == nullptr)
		{
			logte("Could not create application for [%s]", app_info.GetName().CStr());
			// It may not be a error that the Application failed due to disabling that Publisher.
			// Failure to create a single application should not affect the whole.
			// TODO(Getroot): The reason for the error must be defined and handled in detail.
			return true;
		}

		// Connect created application to router
		if(_router->RegisterConnectorApp(*application.get(), application) == false)
		{
			logte("Could not register the application: %p", application.get());
			return false;
		}

		// Store created application
		_applications[application->GetId()] = application;

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

		bool result = OnDeleteProviderApplication(item->second);

		if(result == false)
		{
			logte("Could not delete the application: [%s]", app_info.GetName().CStr());
			return false;
		}

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

	void Provider::RegularTask()
	{
		while(_run_thread)
		{
			for(auto const &x : _applications)
			{
				auto app = x.second;

				// Check if there are terminated streams and delete it
				app->DeleteTerminatedStreams();

				// Check if there are streams have no any viewers
				for(auto const &s : app->GetStreams())
				{
					auto stream = s.second;
					auto stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
					if(stream_metrics != nullptr)
					{
						auto current = std::chrono::high_resolution_clock::now();
        				auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current - stream_metrics->GetLastSentTime()).count();
						
						if(elapsed_time > 30)
						{
							OnStreamNotInUse(*stream);
						}
					}
				}
			}

			sleep(1);
		}
	}
}