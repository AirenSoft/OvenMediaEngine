#include "publisher.h"
#include "publisher_private.h"

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
								ov::Converter::ToString(GetPublisherType()).CStr(), app_info.GetName().CStr());
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

			logte("%s publihser hasn't the %s application.", ov::Converter::ToString(GetPublisherType()).CStr(), app_info.GetName().CStr());
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
			logte("Could not delete the %s application of the %s publisher", app_info.GetName().CStr(), ov::Converter::ToString(GetPublisherType()).CStr());
			return false;
		}

		return true;
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

	std::shared_ptr<Stream> Publisher::PullStream(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &app_name, const ov::String &stream_name, const std::shared_ptr<const ov::Url> &request)
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
			if(request != nullptr)
			{
				auto &query_map = request->QueryMap();
				auto rtsp_uri_item = query_map.find("rtspURI");
				if (rtsp_uri_item == query_map.end())
				{
					logte("There is no rtspURI parameter in the query string: %s", request->ToString().CStr());

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
			if(orchestrator->RequestPullStream(vhost_app_name, host_name, app_name, stream_name) == false)
			{
				return nullptr;
			}
		}
		else
		{
			if(orchestrator->RequestPullStream(vhost_app_name, host_name, app_name, stream_name, pull_url) == false)
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

	SignedUrlErrCode Publisher::HandleSignedUrl(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address, std::shared_ptr<const SignedUrl> &signed_url, ov::String &err_message)
	{
		auto orchestrator = ocst::Orchestrator::GetInstance();
		auto &server_config = GetServerConfig();
		auto vhost_name = orchestrator->GetVhostNameFromDomain(request_url->Domain());

		if (vhost_name.IsEmpty())
		{
			err_message.Format("Could not resolve the domain: %s", request_url->Domain().CStr());
			return SignedUrlErrCode::Unexpected;
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

			// Handle Signed URL if needed
			auto &signed_url_config = vhost_item.GetSignedUrl();
			if (!signed_url_config.IsParsed() || signed_url_config.GetCryptoKey().IsEmpty())
			{
				// The vhost doesn't use the signed url feature.
				return SignedUrlErrCode::Pass;
			}

			// Load config (crypto key, query string key)
			// TODO(Getroot): load signed url type from config and apply them
			auto signed_type = SignedUrlType::Type0;
			auto crypto_key = signed_url_config.GetCryptoKey();
			auto query_string_key = signed_url_config.GetQueryStringKey();

			// Find a signed key in the query string
			auto &query_map = request_url->QueryMap();
			auto item = query_map.find(query_string_key);
			if (item == query_map.end())
			{
				return SignedUrlErrCode::NoSingedKey;
			}

			// Decoding and parsing
			signed_url = SignedUrl::Load(signed_type, crypto_key, item->second);
			if (signed_url == nullptr)
			{
				err_message.Format("Could not obtain decrypted information of the signed url: %s, key: %s, value: %s", request_url->Source().CStr(), query_string_key.CStr(), item->second.CStr());
				return SignedUrlErrCode::DecryptFailed;
			}

			// Check conditions

			if(signed_url->IsTokenExpired())
			{
				err_message.Format("Token is expired: %lld (Now: %lld)", signed_url->GetTokenExpiredTime(), ov::Clock::NowMS());
				return SignedUrlErrCode::TokenExpired;
			}

			if(signed_url->IsStreamExpired())
			{
				err_message.Format("Stream is expired: %lld (Now: %lld)", signed_url->GetStreamExpiredTime(), ov::Clock::NowMS());
				return SignedUrlErrCode::StreamExpired;
			}

			// Check client ip
			if(signed_url->IsAllowedClient(*client_address) == false)
			{
				err_message.Format("Not allowed: %s (Expected: %s)", client_address->ToString().CStr(), signed_url->GetClientIP().CStr());
				return SignedUrlErrCode::UnauthorizedClient;
			}

			// Check URL is same
			// remake url except for signed key
			auto url_to_compare = request_url->ToUrlString(false);
			url_to_compare.Append("?");
			for(const auto &query : query_map)
			{
				auto key = query.first;
				auto value = query.second;

				// signed key is excluded
				if(key.UpperCaseString() == query_string_key.UpperCaseString())
				{
					continue;
				}

				url_to_compare.AppendFormat("%s=%s", key.CStr(), ov::Url::Encode(value).CStr());
			}

			if(url_to_compare.UpperCaseString() != signed_url->GetUrl().UpperCaseString())
			{
				err_message.Format("Invalid URL: %s (Expected: %s)",	signed_url->GetUrl().CStr(), url_to_compare.CStr());
				return SignedUrlErrCode::WrongUrl;
			}

			return SignedUrlErrCode::Success;
		}

		return SignedUrlErrCode::Unexpected;
	}

}  // namespace pub