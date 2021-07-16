#include "server_metrics.h"

namespace mon
{
	void ServerMetrics::ShowInfo()
	{
		for(const auto &t : _hosts)
		{
			auto &host = t.second;
			host->ShowInfo();
		}
	}

	void ServerMetrics::Release()
	{
		for(const auto &host : _hosts)
		{
			host.second->Release();
		}
	}

	std::chrono::system_clock::time_point ServerMetrics::GetServerStartedTime()
	{
		return _server_started_time;
	}

	std::map<uint32_t, std::shared_ptr<HostMetrics>> ServerMetrics::GetHostMetricsList()
	{
		std::shared_lock<std::shared_mutex> lock(_map_guard);
		return _hosts;
	}

	std::shared_ptr<HostMetrics> ServerMetrics::GetHostMetrics(const info::Host &host_info)
	{
		std::shared_lock<std::shared_mutex> lock(_map_guard);
		if (_hosts.find(host_info.GetId()) == _hosts.end())
		{
			return nullptr;
		}

		return _hosts[host_info.GetId()];
	}

	std::shared_ptr<ApplicationMetrics> ServerMetrics::GetApplicationMetrics(const info::Application &app_info)
	{
		auto host_metric = GetHostMetrics(app_info.GetHostInfo());
		if (host_metric == nullptr)
		{
			return nullptr;
		}

		auto app_metric = host_metric->GetApplicationMetrics(app_info);
		if (app_metric == nullptr)
		{
			return nullptr;
		}

		return app_metric;
	}

	std::shared_ptr<StreamMetrics> ServerMetrics::GetStreamMetrics(const info::Stream &stream)
	{
		auto app_metric = GetApplicationMetrics(stream.GetApplicationInfo());
		if (app_metric == nullptr)
		{
			return nullptr;
		}

		auto stream_metric = app_metric->GetStreamMetrics(stream);
		return stream_metric;
	}
}