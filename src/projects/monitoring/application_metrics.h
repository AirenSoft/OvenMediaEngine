//
// Created by getroot on 20. 1. 16.
//
#pragma once

#include "base/common_types.h"
#include "base/info/info.h"
#include "common_metrics.h"
#include "stream_metrics.h"

namespace mon
{
	class HostMetrics;
	class ApplicationMetrics : public info::Application, public CommonMetrics, public ov::EnableSharedFromThis<ApplicationMetrics>
	{
	public:
		ApplicationMetrics(const std::shared_ptr<HostMetrics> &host_metrics, const info::Application &app_info)
			: info::Application(app_info),
			  _host_metrics(host_metrics)
		{
		}
		std::shared_ptr<HostMetrics> GetHostMetrics()
		{
			return _host_metrics;
		}

		bool OnStreamCreated(const info::Stream &stream);
		bool OnStreamDeleted(const info::Stream &stream);

		std::shared_ptr<StreamMetrics> GetStreamMetrics(const info::Stream &stream);

	private:
		std::shared_ptr<HostMetrics> _host_metrics;
		std::mutex _map_guard;
		std::map<uint32_t, std::shared_ptr<StreamMetrics>> _streams;
	};
}  // namespace mon