#include "server_metrics.h"
#include "monitoring_private.h"

namespace mon
{
	ServerMetrics::ServerMetrics(const std::shared_ptr<const cfg::Server> &server_config)
			: _server_config(server_config)
	{
		_server_started_time = std::chrono::system_clock::now();
	}

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

	std::shared_ptr<const cfg::Server> ServerMetrics::GetConfig()
	{
		return _server_config;
	}

	bool ServerMetrics::OnHostCreated(const info::Host &host_info)
	{
		std::unique_lock<std::shared_mutex> lock(_map_guard);
		if(_hosts.find(host_info.GetId()) != _hosts.end())
		{
			return true;
		}
		auto host_metrics = std::make_shared<HostMetrics>(host_info);
		if (host_metrics == nullptr)
		{
			logte("Cannot create HostMetrics (%s/%s)", host_info.GetName().CStr(), host_info.GetUUID().CStr());
			return false;
		}
		
		_hosts[host_info.GetId()] = host_metrics;

		logti("Create HostMetrics(%s/%s) for monitoring", host_info.GetName().CStr(), host_info.GetUUID().CStr());

		return true;
	}
	
	bool ServerMetrics::OnHostDeleted(const info::Host &host_info)
	{
		std::unique_lock<std::shared_mutex> lock(_map_guard);
		auto it = _hosts.find(host_info.GetId());

		if (it == _hosts.end())
		{
			return false;
		}

		auto host = it->second;
		_hosts.erase(it);
		host->Release();

		logti("Delete HostMetrics(%s/%s) for monitoring", host_info.GetName().CStr(), host_info.GetUUID().CStr());

		return true;
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
}