//
// Created by getroot on 20. 1. 16.
//

#include "monitoring.h"
#include "monitoring_private.h"


namespace mon
{
	void Monitoring::SetLogPath(const ov::String &log_path)
	{
		_logger.SetLogPath(log_path);
	}	

	void Monitoring::OnServerStarted(ov::String user_key, ov::String server_name, ov::String server_id)
	{
		_user_key = user_key;
		_server_name = server_name;
		_server_id = server_id;
		_server_started_time = std::chrono::system_clock::now();

		logti("%s(%s) ServerMetric has been started for monitoring - %s", _server_name.CStr(), _server_id.CStr(), ov::Converter::ToISO8601String(_server_started_time));

		auto event = Event(EventType::ServerStarted, _user_key, _server_id);
		_logger.Write(event);
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
			logte("Cannot create HostMetrics (%s/%s)", host_info.GetName().CStr(), host_info.GetUUID().CStr());
			return false;
		}
		
		_hosts[host_info.GetId()] = host_metrics;

		logti("Create HostMetrics(%s/%s) for monitoring", host_info.GetName().CStr(), host_info.GetUUID().CStr());


		auto event = Event(EventType::HostCreated, _user_key, _server_id);
		event.SetMetric(host_metrics);
		_logger.Write(event);

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

		logti("Delete HostMetrics(%s/%s) for monitoring", host_info.GetName().CStr(), host_info.GetUUID().CStr());

		auto event = Event(EventType::HostDeleted, _user_key, _server_id);
		event.SetMetric(host);
		_logger.Write(event);

		return true;
	}
	bool Monitoring::OnApplicationCreated(const info::Application &app_info)
	{
		auto host_metrics = GetHostMetrics(app_info.GetHostInfo());
		if (host_metrics == nullptr)
		{
			return false;
		}

		if(host_metrics->OnApplicationCreated(app_info) == false)
		{
			return false;
		}

		auto app_metrics = host_metrics->GetApplicationMetrics(app_info);
		if(app_metrics == nullptr)
		{
			return false;
		}

		auto event = Event(EventType::AppCreated, _user_key, _server_id);
		event.SetMetric(app_metrics);
		_logger.Write(event);

		return true;
	}
	bool Monitoring::OnApplicationDeleted(const info::Application &app_info)
	{
		auto host_metrics = GetHostMetrics(app_info.GetHostInfo());
		if (host_metrics == nullptr)
		{
			return false;
		}
		auto app_metrics = host_metrics->GetApplicationMetrics(app_info);
		if(app_metrics == nullptr)
		{
			return false;
		}

		if(host_metrics->OnApplicationDeleted(app_info) == false)
		{
			return false;
		}

		auto event = Event(EventType::AppDeleted, _user_key, _server_id);
		event.SetMetric(app_metrics);
		_logger.Write(event);

		return true;
	}
	bool Monitoring::OnStreamCreated(const info::Stream &stream)
	{
		auto app_metrics = GetApplicationMetrics(stream.GetApplicationInfo());
		if (app_metrics == nullptr)
		{
			return false;
		}

		if(app_metrics->OnStreamCreated(stream) == false)
		{
			return false;
		}

		auto stream_metrics = app_metrics->GetStreamMetrics(stream);
		if(stream_metrics == nullptr)
		{
			return false;
		}

		auto event = Event(EventType::StreamCreated, _user_key, _server_id);
		event.SetMetric(stream_metrics);
		_logger.Write(event);

		return true;
	}
	bool Monitoring::OnStreamDeleted(const info::Stream &stream)
	{
		auto app_metrics = GetApplicationMetrics(stream.GetApplicationInfo());
		if (app_metrics == nullptr)
		{
			return false;
		}

		auto stream_metrics = app_metrics->GetStreamMetrics(stream);
		if(stream_metrics == nullptr)
		{
			return false;
		}

		if(app_metrics->OnStreamDeleted(stream) == false)
		{
			return false;
		}

		auto event = Event(EventType::StreamDeleted, _user_key, _server_id);
		event.SetMetric(stream_metrics);
		_logger.Write(event);

		return true;
	}

	bool Monitoring::OnStreamUpdated(const info::Stream &stream_info)
	{
		return true;
	}	

	void Monitoring::IncreaseBytesIn(const info::Stream &stream_info, uint64_t value)
	{
		auto host_metric = GetHostMetrics(stream_info.GetApplicationInfo().GetHostInfo());
		if(host_metric == nullptr)
		{
			return;
		}
		auto app_metric = host_metric->GetApplicationMetrics(stream_info.GetApplicationInfo());
		if(app_metric == nullptr)
		{
			return;
		}
		auto stream_metric = app_metric->GetStreamMetrics(stream_info);
		if(stream_metric == nullptr)
		{
			return;
		}

		host_metric->IncreaseBytesIn(value);
		app_metric->IncreaseBytesIn(value);
		stream_metric->IncreaseBytesIn(value);
	}

	void Monitoring::IncreaseBytesOut(const info::Stream &stream_info, PublisherType type, uint64_t value)
	{
		auto host_metric = GetHostMetrics(stream_info.GetApplicationInfo().GetHostInfo());
		if(host_metric == nullptr)
		{
			return;
		}
		auto app_metric = host_metric->GetApplicationMetrics(stream_info.GetApplicationInfo());
		if(app_metric == nullptr)
		{
			return;
		}
		auto stream_metric = app_metric->GetStreamMetrics(stream_info);
		if(stream_metric == nullptr)
		{
			return;
		}

		host_metric->IncreaseBytesOut(type, value);
		app_metric->IncreaseBytesOut(type, value);
		stream_metric->IncreaseBytesOut(type, value);
	}

	void Monitoring::OnSessionConnected(const info::Stream &stream_info, PublisherType type)
	{
		auto host_metric = GetHostMetrics(stream_info.GetApplicationInfo().GetHostInfo());
		if(host_metric == nullptr)
		{
			return;
		}
		auto app_metric = host_metric->GetApplicationMetrics(stream_info.GetApplicationInfo());
		if(app_metric == nullptr)
		{
			return;
		}
		auto stream_metric = app_metric->GetStreamMetrics(stream_info);
		if(stream_metric == nullptr)
		{
			return;
		}

		host_metric->OnSessionConnected(type);
		app_metric->OnSessionConnected(type);
		stream_metric->OnSessionConnected(type);
	}

	void Monitoring::OnSessionDisconnected(const info::Stream &stream_info, PublisherType type)
	{
		auto host_metric = GetHostMetrics(stream_info.GetApplicationInfo().GetHostInfo());
		if(host_metric == nullptr)
		{
			return;
		}
		auto app_metric = host_metric->GetApplicationMetrics(stream_info.GetApplicationInfo());
		if(app_metric == nullptr)
		{
			return;
		}
		auto stream_metric = app_metric->GetStreamMetrics(stream_info);
		if(stream_metric == nullptr)
		{
			return;
		}

		host_metric->OnSessionDisconnected(type);
		app_metric->OnSessionDisconnected(type);
		stream_metric->OnSessionDisconnected(type);
	}

	void Monitoring::OnSessionsDisconnected(const info::Stream &stream_info, PublisherType type, uint64_t number_of_sessions)
	{
		auto host_metric = GetHostMetrics(stream_info.GetApplicationInfo().GetHostInfo());
		if(host_metric == nullptr)
		{
			return;
		}
		auto app_metric = host_metric->GetApplicationMetrics(stream_info.GetApplicationInfo());
		if(app_metric == nullptr)
		{
			return;
		}
		auto stream_metric = app_metric->GetStreamMetrics(stream_info);
		if(stream_metric == nullptr)
		{
			return;
		}

		host_metric->OnSessionsDisconnected(type, number_of_sessions);
		app_metric->OnSessionsDisconnected(type, number_of_sessions);
		stream_metric->OnSessionsDisconnected(type, number_of_sessions);
	}

}  // namespace mon