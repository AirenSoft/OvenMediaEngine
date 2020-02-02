//
// Created by getroot on 20. 1. 16.
//

#pragma once

#include "application_metrics.h"
#include "base/common_types.h"
#include "base/info/host.h"
#include "common_metrics.h"

namespace mon
{
	class HostMetrics : public info::Host, public CommonMetrics, public ov::EnableSharedFromThis<HostMetrics>
	{
	public:
		HostMetrics(const info::Host &host_info)
			: info::Host(host_info)
		{
		}
		bool OnApplicationCreated(const info::Application &app_info);
		bool OnApplicationDeleted(const info::Application &app_info);

		std::shared_ptr<ApplicationMetrics> GetApplicationMetrics(const info::Application &app_info);

	private:
		std::mutex _map_guard;
		std::map<uint32_t, std::shared_ptr<ApplicationMetrics>> _applications;
	};
}  // namespace mon