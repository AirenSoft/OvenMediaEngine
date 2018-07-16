#include "publisher_private.h"
#include "publisher.h"

Publisher::Publisher(std::shared_ptr<MediaRouteInterface> router)
{
	_router = router;
}

Publisher::~Publisher()
{
}

bool Publisher::Start(std::vector<std::shared_ptr<ApplicationInfo>> &application_infos)
{
	for(auto const &application_info : application_infos)
	{
		auto publishers = application_info->GetPublishers();
		for(auto const &publisher : publishers)
		{
			if(GetPublisherType() == publisher->GetType())
			{
				logti("Create application : %s", application_info->GetName().CStr());
				auto application = OnCreateApplication(application_info);
				// 생성한 Application을 Router와 연결하고 Start
				_router->RegisterObserverApp(application_info, application);
				// Apllication Map에 보관
				_applications[application->GetId()] = application;

				break;
			}
		}
	}
}

bool Publisher::Stop()
{
	auto it = _applications.begin();
	while(it != _applications.end())
	{
		auto application = it->second;

		_router->UnregisterObserverApp(application, application);

		it = _applications.erase(it);
	}
}

std::shared_ptr<Application> Publisher::GetApplication(ov::String app_name)
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

std::shared_ptr<Stream> Publisher::GetStream(ov::String app_name, ov::String stream_name)
{
	auto app = GetApplication(app_name);
	if(!app)
	{
		return nullptr;
	}

	return app->GetStream(stream_name);
}

std::shared_ptr<Application> Publisher::GetApplication(uint32_t app_id)
{
	if(_applications.count(app_id) <= 0)
	{
		return nullptr;
	}

	return _applications[app_id];
}

std::shared_ptr<Stream> Publisher::GetStream(uint32_t app_id, uint32_t stream_id)
{
	auto app = GetApplication(app_id);
	if(!app)
	{
		return nullptr;
	}

	return app->GetStream(stream_id);
}