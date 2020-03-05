#include "publisher.h"
#include "publisher_private.h"

namespace pub
{
	Publisher::Publisher(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
		: _server_config(server_config),
		  _host_info(host_info),
		  _router(router)
	{
	}

	bool Publisher::Start()
	{
		logtd("Trying to start %s publisher (%d)", GetPublisherName(), GetPublisherType());

		return true;
	}

	bool Publisher::Stop()
	{
		auto it = _applications.begin();

		while (it != _applications.end())
		{
			auto application = it->second;

			_router->UnregisterObserverApp(*application.get(), application);

			it = _applications.erase(it);
		}

		return true;
	}

	const cfg::Server &Publisher::GetServerConfig() const
	{
		return _server_config;
	}

	const info::Host &Publisher::GetHostInfo() const
	{
		return _host_info;
	}

	// Create Application
	bool Publisher::OnCreateApplication(const info::Application &app_info)
	{
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
		_applications[application->GetId()] = application;

		return true;
	}

	// Delete Application
	bool Publisher::OnDeleteApplication(const info::Application &app_info)
	{
		if (_applications.find(app_info.GetId()) == _applications.end())
		{
			return true;
		}

		_applications[app_info.GetId()]->Stop();
		_applications.erase(app_info.GetId());

		return true;
	}

	std::shared_ptr<Application> Publisher::GetApplicationByName(ov::String app_name)
	{
		for (auto const &x : _applications)
		{
			auto application = x.second;
			if (application->GetName() == app_name)
			{
				return application;
			}
		}

		return nullptr;
	}

	std::shared_ptr<Stream> Publisher::GetStream(ov::String app_name, ov::String stream_name)
	{
		auto app = GetApplicationByName(std::move(app_name));

		if (app != nullptr)
		{
			return app->GetStream(std::move(stream_name));
		}

		return nullptr;
	}

	std::shared_ptr<Application> Publisher::GetApplicationById(info::application_id_t application_id)
	{
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