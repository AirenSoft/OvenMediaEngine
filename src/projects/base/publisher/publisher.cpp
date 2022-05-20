#include "publisher.h"
#include "publisher_private.h"
#include <orchestrator/orchestrator.h>

namespace pub
{
	Publisher::Publisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: _server_config(server_config), _router(router)
	{
	}

	Publisher::~Publisher()
	{
	}

	bool Publisher::Start()
	{
		logti("%s has been started.", GetPublisherName());

		SetModuleAvailable(true);

		_access_controller = std::make_shared<AccessController>(GetPublisherType(), GetServerConfig());

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
		SetModuleAvailable(false);
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
						logtw("%s publisher is disabled in %s application, so it was not created", 
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
		if (_router->RegisterObserverApp(*application.get(), application) == false)
		{
			logte("Failed to register application(%s/%s) to router", GetPublisherName(), app_info.GetName().CStr());
			return false;
		}
		

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
		ov::String desire_stream_name = stream_name;

		// TODO(dimiden): This is temporalily codes, needs to be removed in the future
		// Orchestrator::RequestPullStream should receive ov::Url instead of rtsp_uri in the future and
		// extract rtsp_uri by itself according to the configuration.
		if (vapp_name.HasSuffix("#rtsp_live") || vapp_name.HasSuffix("#rtsp_playback") ||
			vapp_name.HasSuffix("#rtsp_live_insecure") || vapp_name.HasSuffix("#rtsp_playback_insecure"))
		{
			if (request_from != nullptr)
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
#if 0
		//	Unit test code for pull request
		else
		{
			if (request_from != nullptr)
			{
				auto &query_map = request_from->QueryMap();
				auto pull_uri_item = query_map.find("pullURI");
				if (pull_uri_item != query_map.end())
				{
					pull_url = pull_uri_item->second;
				}

				auto target_stream_item = query_map.find("targetStream");
				if (target_stream_item != query_map.end())
				{
					desire_stream_name = target_stream_item->second;
					logtd("Replace source stream name %s to %s", stream_name.CStr(), desire_stream_name.CStr());
				}
			}
		}
#endif

		if (pull_url.IsEmpty())
		{
			if(orchestrator->RequestPullStream(request_from, vhost_app_name, desire_stream_name) == false)
			{
				return nullptr;
			}
		}
		else
		{
			if(orchestrator->RequestPullStream(request_from, vhost_app_name, desire_stream_name, pull_url) == false)
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

	bool Publisher::IsAccessControlEnabled(const std::shared_ptr<const ov::Url> &request_url)
	{
		auto orchestrator = ocst::Orchestrator::GetInstance();
		auto vhost_name = orchestrator->GetVhostNameFromDomain(request_url->Host());

		if (vhost_name.IsEmpty())
		{
			return false;
		}

		auto item = ocst::Orchestrator::GetInstance()->GetHostInfo(vhost_name);
		if (item.has_value())
		{
			auto vhost_item = item.value();

			auto &webhooks_config = vhost_item.GetAdmissionWebhooks();
			auto &signed_policy_config = vhost_item.GetSignedPolicy();
			if (webhooks_config.IsParsed() == true)
			{
				if (webhooks_config.IsEnabledPublisher(GetPublisherType()) == true)
				{
					return true;
				}
			}

			if (signed_policy_config.IsParsed() == true)
			{
				if (signed_policy_config.IsEnabledPublisher(GetPublisherType()) == true)
				{
					return true;
				}
			}
		}

		return false;
	}

	std::tuple<AccessController::VerificationResult, std::shared_ptr<const SignedPolicy>> Publisher::VerifyBySignedPolicy(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address)
	{
		if(_access_controller == nullptr)
		{
			return {AccessController::VerificationResult::Error, nullptr};
		}

		return _access_controller->VerifyBySignedPolicy(request_url, client_address);
	}

	std::tuple<AccessController::VerificationResult, std::shared_ptr<const AdmissionWebhooks>> Publisher::SendCloseAdmissionWebhooks(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address)
	{
		if(_access_controller == nullptr)
		{
			return {AccessController::VerificationResult::Error, nullptr};
		}

		return _access_controller->SendCloseWebhooks(request_url, client_address);
	}

	std::tuple<AccessController::VerificationResult, std::shared_ptr<const AdmissionWebhooks>> Publisher::VerifyByAdmissionWebhooks(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address)
	{
		if(_access_controller == nullptr)
		{
			return {AccessController::VerificationResult::Error, nullptr};
		}

		return _access_controller->VerifyByWebhooks(request_url, client_address);
	}

	std::tuple<AccessController::VerificationResult, std::shared_ptr<const SignedToken>>  Publisher::VerifyBySignedToken(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address)
	{
		if(_access_controller == nullptr)
		{
			return {AccessController::VerificationResult::Error, nullptr};
		}

		return _access_controller->VerifyBySignedToken(request_url, client_address);
	}

}  // namespace pub
