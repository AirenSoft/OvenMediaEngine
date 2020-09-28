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

}  // namespace pub