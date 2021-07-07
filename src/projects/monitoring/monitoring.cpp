//
// Created by getroot on 20. 1. 16.
//

#include "monitoring.h"
#include "monitoring_private.h"
#include "base/ovlibrary/uuid.h"
#include <fstream>

namespace mon
{
	void Monitoring::ShowInfo()
	{
		for(const auto &t : _hosts)
		{
			auto &host = t.second;
			host->ShowInfo();
		}
	}

	void Monitoring::SetServerName(ov::String name)
	{
		_server_name = name;
	}

	ov::String Monitoring::GetServerID()
	{
		if(_server_id.IsEmpty() == false)
		{
			return _server_id;
		}

		{
			auto [result, server_id] = LoadServerIDFromStorage();
			if(result == true)
			{
				_server_id = server_id;
				return server_id;
			}
		}

		{
			auto [result, server_id] = GenerateServerID();
			if(result == true)
			{
				StoreServerID(server_id);
				return server_id;
			}
		}

		return "";
	}

	std::tuple<bool, ov::String> Monitoring::LoadServerIDFromStorage() const
	{
		// If node id is empty, try to load ID from file
		auto exe_path = ov::PathManager::GetAppPath();
		auto node_id_storage = ov::PathManager::Combine(exe_path, SERVER_ID_STORAGE_FILE);

		std::ifstream fs(node_id_storage);
		if(!fs.is_open())
		{
			return {false, ""};
		}

		std::string line;
		std::getline(fs, line);
		fs.close();

		line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

		return {true, line.c_str()};
	}

	bool Monitoring::StoreServerID(ov::String server_id)
	{
		_server_id = server_id;

		// Store server_id to storage
		auto exe_path = ov::PathManager::GetAppPath();
		auto node_id_storage = ov::PathManager::Combine(exe_path, SERVER_ID_STORAGE_FILE);

		std::ofstream fs(node_id_storage);
		if(!fs.is_open())
		{
			return false;
		}

		fs.write(server_id.CStr(), server_id.GetLength());
		fs.close();
		return true;
	}

	std::tuple<bool, ov::String> Monitoring::GenerateServerID() const
	{
		auto uuid = ov::UUID::Generate();
		auto server_id = ov::String::FormatString("%s_%s", _server_name.CStr(), uuid.CStr());
		return {true, server_id};
	}

	void Monitoring::Release()
	{
		for(const auto &host : _hosts)
		{
			host.second->Release();
		}
	}
	
	bool Monitoring::OnHostCreated(const info::Host &host_info)
	{
		std::unique_lock<std::shared_mutex> lock(_map_guard);
		if(_hosts.find(host_info.GetId()) != _hosts.end())
		{
			return true;
		}
		auto host_metrics = std::make_shared<HostMetrics>(host_info);
		if (host_metrics == nullptr)
		{
			logte("Cannot create HostMetrics (%s)", host_info.GetName().CStr());
			return false;
		}
		
		_hosts[host_info.GetId()] = host_metrics;

		logti("Create HostMetrics(%s) for monitoring", host_info.GetName().CStr());
		return true;
	}
	bool Monitoring::OnHostDeleted(const info::Host &host_info)
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

		logti("Delete HostMetrics(%s) for monitoring", host_info.GetName().CStr());
		return true;
	}
	bool Monitoring::OnApplicationCreated(const info::Application &app_info)
	{
		auto host_metrics = GetHostMetrics(app_info.GetHostInfo());
		if (host_metrics == nullptr)
		{
			return false;
		}

		return host_metrics->OnApplicationCreated(app_info);
	}
	bool Monitoring::OnApplicationDeleted(const info::Application &app_info)
	{
		auto host_metrics = GetHostMetrics(app_info.GetHostInfo());
		if (host_metrics == nullptr)
		{
			return false;
		}

		return host_metrics->OnApplicationDeleted(app_info);
	}
	bool Monitoring::OnStreamCreated(const info::Stream &stream)
	{
		auto app_metrics = GetApplicationMetrics(stream.GetApplicationInfo());
		if (app_metrics == nullptr)
		{
			return false;
		}

		return app_metrics->OnStreamCreated(stream);
	}
	bool Monitoring::OnStreamDeleted(const info::Stream &stream)
	{
		auto app_metrics = GetApplicationMetrics(stream.GetApplicationInfo());
		if (app_metrics == nullptr)
		{
			return false;
		}

		return app_metrics->OnStreamDeleted(stream);
	}

	std::map<uint32_t, std::shared_ptr<HostMetrics>> Monitoring::GetHostMetricsList()
	{
		std::shared_lock<std::shared_mutex> lock(_map_guard);
		return _hosts;
	}

	std::shared_ptr<HostMetrics> Monitoring::GetHostMetrics(const info::Host &host_info)
	{
		std::shared_lock<std::shared_mutex> lock(_map_guard);
		if (_hosts.find(host_info.GetId()) == _hosts.end())
		{
			return nullptr;
		}

		return _hosts[host_info.GetId()];
	}

	std::shared_ptr<ApplicationMetrics> Monitoring::GetApplicationMetrics(const info::Application &app_info)
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

	std::shared_ptr<StreamMetrics> Monitoring::GetStreamMetrics(const info::Stream &stream)
	{
		auto app_metric = GetApplicationMetrics(stream.GetApplicationInfo());
		if (app_metric == nullptr)
		{
			return nullptr;
		}

		auto stream_metric = app_metric->GetStreamMetrics(stream);
		return stream_metric;
	}
}  // namespace mon