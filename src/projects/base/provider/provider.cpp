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
#include "provider_private.h"

#include <orchestrator/orchestrator.h>

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
		logti("%s has been started.", GetProviderName());

		return true;
	}

	bool Provider::Stop()
	{
		std::unique_lock<std::shared_mutex> lock(_application_map_mutex);

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
								::StringFromProviderType(GetProviderType()).CStr(), app_info.GetName().CStr());
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

		std::unique_lock<std::shared_mutex> lock(_application_map_mutex);
		// Store created application
		_applications[application->GetId()] = application;

		return true;
	}

	// Delete Application
	bool Provider::OnDeleteApplication(const info::Application &app_info)
	{
		std::unique_lock<std::shared_mutex> lock(_application_map_mutex);
		auto item = _applications.find(app_info.GetId());

		logtd("Delete the application: [%s]", app_info.GetName().CStr());
		if(item == _applications.end())
		{
			// Check the reason the app is not created is because it is disabled in the configuration
			if(app_info.IsDynamicApp() == false)
			{
				auto cfg_provider_list = app_info.GetConfig().GetProviders().GetProviderList();
				for(const auto &cfg_provider : cfg_provider_list)
				{
					if(cfg_provider->GetType() == GetProviderType())
					{
						// this provider is disabled
						if(!cfg_provider->IsParsed())
						{
							return true;
						}
					}
				}
			}

			logte("%s provider hasn't the %s application.", ::StringFromProviderType(GetProviderType()).CStr(), app_info.GetName().CStr());
			return false;
		}

		auto application = item->second;

		// Disconnect deleted application to router
		if(_router->UnregisterConnectorApp(*application.get(), application) == false)
		{
			logte("Could not unregister the application: %p", application.get());
			return false;
		}

		_applications.erase(item);

		lock.unlock();

		bool result = OnDeleteProviderApplication(application);
		if(result == false)
		{
			logte("Could not delete [%s] the application of the %s provider", app_info.GetName().CStr(), ::StringFromProviderType(GetProviderType()).CStr());
			return false;
		}

		application->Stop();

		return true;
	}

	std::shared_ptr<Application> Provider::GetApplicationByName(const info::VHostAppName &vhost_app_name)
	{
		std::shared_lock<std::shared_mutex> lock(_application_map_mutex);

		for(auto const &x : _applications)
		{
			auto application = x.second;
			if(application->GetName() == vhost_app_name)
			{
				return application;
			}
		}

		return nullptr;
	}

	std::shared_ptr<Stream> Provider::GetStreamByName(const info::VHostAppName &vhost_app_name, ov::String stream_name)
	{
		auto app = GetApplicationByName(vhost_app_name);
		if(!app)
		{
			return nullptr;
		}

		return app->GetStreamByName(stream_name);
	}

	std::shared_ptr<Application> Provider::GetApplicationById(info::application_id_t application_id)
	{
		std::shared_lock<std::shared_mutex> lock(_application_map_mutex);

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

	CheckSignatureResult Provider::HandleSignedPolicy(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address, std::shared_ptr<const SignedPolicy> &signed_policy)
	{
		auto orchestrator = ocst::Orchestrator::GetInstance();
		auto &server_config = GetServerConfig();
		auto vhost_name = orchestrator->GetVhostNameFromDomain(request_url->Host());

		if (vhost_name.IsEmpty())
		{
			logte("Could not resolve the domain: %s", request_url->Host().CStr());
			return CheckSignatureResult::Error;
		}

		// TODO(Dimiden) : Modify below codes
		// GetVirtualHostByName is deprecated so blow codes are insane, later it will be modified.
		auto vhost_list = server_config.GetVirtualHostList();
		for (const auto &vhost_item : vhost_list)
		{
			if (vhost_item.GetName() != vhost_name)
			{
				continue;
			}

			// Handle SignedPolicy if needed
			auto &signed_policy_config = vhost_item.GetSignedPolicy();
			if (!signed_policy_config.IsParsed())
			{
				// The vhost doesn't use the SignedPolicy  feature.
				return CheckSignatureResult::Off;
			}

			if(signed_policy_config.IsEnabledProvider(GetProviderType()) == false)
			{
				// This publisher turned off the SignedPolicy function
				return CheckSignatureResult::Off;
			}

			auto policy_query_key_name = signed_policy_config.GetPolicyQueryKeyName();
			auto signature_query_key_name = signed_policy_config.GetSignatureQueryKeyName();
			auto secret_key = signed_policy_config.GetSecretKey();

			signed_policy = SignedPolicy::Load(client_address->ToString(), request_url->ToUrlString(), policy_query_key_name, signature_query_key_name, secret_key);
			if(signed_policy == nullptr)
			{
				// Probably this doesn't happen
				logte("Could not load SingedToken");
				return CheckSignatureResult::Error;
			}

			if(signed_policy->GetErrCode() != SignedPolicy::ErrCode::PASSED)
			{
				return CheckSignatureResult::Fail;
			}

			return CheckSignatureResult::Pass;
		}

		// Probably this doesn't happen
		logte("Could not find VirtualHost (%s)", vhost_name);
		return CheckSignatureResult::Error;
	}
}