//
// Created by getroot on 20. 1. 16.
//

#pragma once

#include "base/info/host.h"
#include "base/info/info.h"
#include "host_metrics.h"
#include <shared_mutex>

#define MonitorInstance				mon::Monitoring::GetInstance()
#define HostMetrics(info)			mon::Monitoring::GetInstance()->GetHostMetrics(info);
#define ApplicationMetrics(info)	mon::Monitoring::GetInstance()->GetApplicationMetrics(info);
#define StreamMetrics(info)			mon::Monitoring::GetInstance()->GetStreamMetrics(info);

namespace mon
{
	class Monitoring
	{
	public:
        static Monitoring *GetInstance()
	    {
            static Monitoring monitor;
            return &monitor;
	    }

		void Release();

		void ShowInfo();

		bool OnHostCreated(const info::Host &host_info);
		bool OnHostDeleted(const info::Host &host_info);
		bool OnApplicationCreated(const info::Application &app_info);
		bool OnApplicationDeleted(const info::Application &app_info);
		bool OnStreamCreated(const info::Stream &stream_info);
		bool OnStreamDeleted(const info::Stream &stream_info);

		std::map<uint32_t, std::shared_ptr<HostMetrics>> GetHostMetricsList();
		
        std::shared_ptr<HostMetrics> GetHostMetrics(const info::Host &host_info);
        std::shared_ptr<ApplicationMetrics> GetApplicationMetrics(const info::Application &app_info);
        std::shared_ptr<StreamMetrics>  GetStreamMetrics(const info::Stream &stream_info);

	private:
		std::shared_mutex _map_guard;
		std::map<uint32_t, std::shared_ptr<HostMetrics>> _hosts;
	};
}  // namespace mon