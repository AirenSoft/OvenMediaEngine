//
// Created by getroot on 21. 7. 13.
//

#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include "base/info/host.h"
#include "host_metrics.h"

namespace mon
{
	class ServerMetrics
	{
	public:
		void Release();
		void ShowInfo();
		std::chrono::system_clock::time_point GetServerStartedTime();
		std::map<uint32_t, std::shared_ptr<HostMetrics>> GetHostMetricsList();
        std::shared_ptr<HostMetrics> GetHostMetrics(const info::Host &host_info);
        std::shared_ptr<ApplicationMetrics> GetApplicationMetrics(const info::Application &app_info);
        std::shared_ptr<StreamMetrics>  GetStreamMetrics(const info::Stream &stream_info);

	protected:
		ov::String _server_name;
		ov::String _server_id;
		std::chrono::system_clock::time_point _server_started_time;
		std::shared_mutex _map_guard;
		std::map<uint32_t, std::shared_ptr<HostMetrics>> _hosts;

	};
}