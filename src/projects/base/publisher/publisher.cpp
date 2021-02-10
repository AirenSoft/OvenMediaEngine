#include "publisher.h"
#include "publisher_private.h"
#include <orchestrator/orchestrator.h>

namespace pub
{
	Publisher::Publisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: _server_config(server_config),
		  _router(router)
	{
	}

	Publisher::~Publisher()
	{
	}

	bool Publisher::Start()
	{
		logti("%s has been started.", GetPublisherName());
		return true;
	}

	bool Publisher::Stop()
	{
		std::unique_lock<std::shared_mutex> lock(_application_map_mutex);
		auto it = _applications.begin();

		while (it != _applications.end())
		{
			auto application = it->second;

			_router->UnregisterObserverApp(*application.get(), application);

			it = _applications.erase(it);
		}

		logti("%s has been stopped.", GetPublisherName());
		return true;
	}

	const cfg::Server &Publisher::GetServerConfig() const
	{
		return _server_config;
	}

	// Create Application
	bool Publisher::OnCreateApplication(const info::Application &app_info)
	{
		// Check configuration
		if(app_info.IsDynamicApp() == false)
		{
			auto cfg_publisher_list = app_info.GetConfig().GetPublishers().GetPublisherList();
			for(const auto &cfg_publisher : cfg_publisher_list)
			{
				if(cfg_publisher->GetType() == GetPublisherType())
				{
					if(cfg_publisher->IsParsed())
					{
						break;
					}
					else
					{
						// This provider is diabled
						logti("%s publisher is disabled in %s application, so it was not created", 
								::StringFromPublisherType(GetPublisherType()).CStr(), app_info.GetName().CStr());
						return true;
					}
				}
			}
		}
		else
		{
			// The dynamically created app activates all publishers
		}


		auto application = OnCreatePublisherApplication(app_info);
		if (application == nullptr)
		{
			// It may not be a error that the Application failed due to disabling that Publisher.
			// Failure to create a single application should not affect the whole.
			// TODO(Getroot): The reason for the error must be defined and handled in detail.
			return true;
		}

		// 생성한 Application을 Router와 연결하고 Start
		_router->RegisterObserverApp(*application.get(), application);

		// Application Map에 보관
		std::lock_guard<std::shared_mutex> lock(_application_map_mutex);
		_applications[application->GetId()] = application;

		return true;
	}

	// Delete Application
	bool Publisher::OnDeleteApplication(const info::Application &app_info)
	{
		std::unique_lock<std::shared_mutex> lock(_application_map_mutex);
		auto item = _applications.find(app_info.GetId());

		logtd("Delete the application: [%s]", app_info.GetName().CStr());
		if(item == _applications.end())
		{
			// Check the reason the app is not created is because it is disabled in the configuration
			if(app_info.IsDynamicApp() == false)
			{
				auto cfg_publisher_list = app_info.GetConfig().GetPublishers().GetPublisherList();
				for(const auto &cfg_publisher : cfg_publisher_list)
				{
					if(cfg_publisher->GetType() == GetPublisherType())
					{
						// this provider is disabled
						if(!cfg_publisher->IsParsed())
						{
							return true;
						}
					}
				}
			}

			logte("%s publisher hasn't the %s application.", ::StringFromPublisherType(GetPublisherType()).CStr(), app_info.GetName().CStr());
			return false;
		}

		auto application = item->second;
		_applications[app_info.GetId()]->Stop();
		_applications.erase(item);

		lock.unlock();

		_router->UnregisterObserverApp(*application.get(), application);
		
		bool result = OnDeletePublisherApplication(application);
		if(result == false)
		{
			logte("Could not delete the %s application of the %s publisher", app_info.GetName().CStr(), ::StringFromPublisherType(GetPublisherType()).CStr());
			return false;
		}

		return true;
	}

	uint32_t Publisher::GetApplicationCount()
	{
		return _applications.size();
	}

	std::shared_ptr<Application> Publisher::GetApplicationByName(const info::VHostAppName &vhost_app_name)
	{
		std::shared_lock<std::shared_mutex> lock(_application_map_mutex);
		for (auto const &x : _applications)
		{
			auto application = x.second;
			if (application->GetName() == vhost_app_name)
			{
				return application;
			}
		}

		return nullptr;
	}

	std::shared_ptr<Stream> Publisher::PullStream(const std::shared_ptr<const ov::Url> &request_from, const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name)
	{
		auto stream = GetStream(vhost_app_name, stream_name);
		if(stream != nullptr)
		{
			return stream;
		}

		auto orchestrator = ocst::Orchestrator::GetInstance();
		auto &vapp_name = vhost_app_name.ToString();
		
		ov::String pull_url;

		// TODO(dimiden): This is temporalily codes, needs to be removed in the future
		// Orchestrator::RequestPullStream should receive ov::Url instead of rtsp_uri in the future and 
		// extract rtsp_uri by itself according to the configuration.
		if(	vapp_name.HasSuffix("#rtsp_live") || vapp_name.HasSuffix("#rtsp_playback") ||
			vapp_name.HasSuffix("#rtsp_live_insecure") || vapp_name.HasSuffix("#rtsp_playback_insecure"))
		{
			if(request_from != nullptr)
			{
				auto &query_map = request_from->QueryMap();
				auto rtsp_uri_item = query_map.find("rtspURI");
				if (rtsp_uri_item == query_map.end())
				{
					logte("There is no rtspURI parameter in the query string: %s", request_from->ToString().CStr());

					logtd("Query map:");
					for ([[maybe_unused]] auto &query : query_map)
					{
						logtd("    %s = %s", query.first.CStr(), query.second.CStr());
					}
				}
				else
				{
					pull_url = rtsp_uri_item->second;
				}
			}
		}
		
		if(pull_url.IsEmpty())
		{
			if(orchestrator->RequestPullStream(request_from, vhost_app_name, stream_name) == false)
			{
				return nullptr;
			}
		}
		else
		{
			if(orchestrator->RequestPullStream(request_from, vhost_app_name, stream_name, pull_url) == false)
			{
				return nullptr;
			}
		}

		// try one more after pulling stream
		return GetStream(vhost_app_name, stream_name);
	}

	std::shared_ptr<Stream> Publisher::GetStream(const info::VHostAppName &vhost_app_name, const ov::String &stream_name)
	{
		auto app = GetApplicationByName(vhost_app_name);
		if (app != nullptr)
		{
			return app->GetStream(stream_name);
		}

		return nullptr;
	}

	std::shared_ptr<Application> Publisher::GetApplicationById(info::application_id_t application_id)
	{
		std::shared_lock<std::shared_mutex> lock(_application_map_mutex);

		auto application = _applications.find(application_id);
		if (application == _applications.end())
		{
			return nullptr;
		}

		return application->second;
	}

	std::shared_ptr<Stream> Publisher::GetStream(info::application_id_t application_id, uint32_t stream_id)
	{
		auto app = GetApplicationById(application_id);
		if (app != nullptr)
		{
			return app->GetStream(stream_id);
		}

		return nullptr;
	}

	CheckSignatureResult Publisher::HandleSignedPolicy(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address, std::shared_ptr<const SignedPolicy> &signed_policy)
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

			if(signed_policy_config.IsEnabledPublisher(GetPublisherType()) == false)
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

	CheckSignatureResult Publisher::HandleSignedToken(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address, std::shared_ptr<const SignedToken> &signed_token)
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

			// Handle SignedToken if needed
			auto &signed_token_config = vhost_item.GetSignedToken();
			if (!signed_token_config.IsParsed() || signed_token_config.GetCryptoKey().IsEmpty())
			{
				// The vhost doesn't use the signed url feature.
				return CheckSignatureResult::Off;
			}

			auto crypto_key = signed_token_config.GetCryptoKey();
			auto query_string_key = signed_token_config.GetQueryStringKey();

			signed_token = SignedToken::Load(client_address->ToString(), request_url->ToUrlString(), query_string_key, crypto_key);
			if (signed_token == nullptr)
			{
				// Probably this doesn't happen
				logte("Could not load SingedToken");
				return CheckSignatureResult::Error;
			}

			if(signed_token->GetErrCode() != SignedToken::ErrCode::PASSED)
			{
				return CheckSignatureResult::Fail;
			}

			return CheckSignatureResult::Pass;
		}

		// Probably this doesn't happen
		logte("Could not find VirtualHost (%s)", vhost_name);
		return CheckSignatureResult::Error;
	}

}  // namespace pub