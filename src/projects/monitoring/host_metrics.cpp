//
// Created by getroot on 20. 1. 16.
//

#include "monitoring_private.h"
#include "host_metrics.h"

namespace mon
{
	bool HostMetrics::OnApplicationCreated(const info::Application &app_info)
	{
		auto app_metrics = std::make_shared<ApplicationMetrics>(GetSharedPtr(), app_info);
        if(app_metrics == nullptr)
        {
            logte("Cannot create ApplicationMetrics (%s/%s)", GetName().CStr(), app_info.GetName().CStr());
            return false;
        }

        std::unique_lock<std::mutex> lock(_map_guard);
        _applications[app_info.GetId()] = app_metrics;

		logti("Create ApplicationMetrics(%s) for monitoring", app_info.GetName().CStr());
        return true;
	}
	bool HostMetrics::OnApplicationDeleted(const info::Application &app_info)
	{
        std::unique_lock<std::mutex> lock(_map_guard);
        if(_applications.find(app_info.GetId()) == _applications.end())
        {
            return false;
        }
        _applications.erase(app_info.GetId());

		logti("Delete ApplicationMetrics(%s) for monitoring", app_info.GetName().CStr());
        return true;
	}

	std::shared_ptr<ApplicationMetrics> HostMetrics::GetApplicationMetrics(const info::Application &app_info)
	{
		std::unique_lock<std::mutex> lock(_map_guard);
		if (_applications.find(app_info.GetId()) == _applications.end())
		{
			return nullptr;
		}

		return _applications[app_info.GetId()];
	}
}  // namespace mon