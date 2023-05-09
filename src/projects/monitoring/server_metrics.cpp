#include "server_metrics.h"

#include "monitoring_private.h"
#include <orchestrator/orchestrator.h>
#include <malloc.h>

namespace mon
{
	ServerMetrics::ServerMetrics(const std::shared_ptr<const cfg::Server> &server_config)
		: _server_config(server_config)
	{
		_server_started_time = std::chrono::system_clock::now();
	}

	void ServerMetrics::ShowInfo()
	{
		for (const auto &t : _hosts)
		{
			auto &host = t.second;
			host->ShowInfo();
		}
	}

	void ServerMetrics::Release()
	{
		for (const auto &host : _hosts)
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
		if (_hosts.find(host_info.GetId()) != _hosts.end())
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

	void ServerMetrics::OnQueueCreated(const info::ManagedQueue &info)
	{
		std::shared_lock<std::shared_mutex> lock(_map_guard);

		auto queue_metrics = std::make_shared<QueueMetrics>(info);
		if (queue_metrics == nullptr)
		{
			logte("Cannot create QueueMetrics (%u/%s)", info.GetId(), info.GetUrn().CStr());
			return;
		}

		_queues[info.GetId()] = queue_metrics;
	}

	void ServerMetrics::OnQueueDeleted(const info::ManagedQueue &info)
	{
		std::shared_lock<std::shared_mutex> lock(_map_guard);

		auto it = _queues.find(info.GetId());
		if (it == _queues.end())
		{
			return;
		}

		[[maybe_unused]] auto queue = it->second;

		_queues.erase(it);
	}

	void ServerMetrics::OnQueueUpdated(const info::ManagedQueue &info, bool with_metadata)
	{
		std::shared_lock<std::shared_mutex> lock(_map_guard);
		
		auto it = _queues.find(info.GetId());
		if (it == _queues.end())
		{
			return;
		}

		auto queue = it->second;

		if (with_metadata == true)
		{
			queue->UpdateMetadata(info);
		}

		queue->UpdateMetadata(info);

		/**
			[Experimental] Delete lazy stream

			If the size of the queue lasts beyond the limit for N seconds, it is determined to be an invalid stream. 
			For system recovery, the problematic stream is forcibly deleted.

			server.xml:
				<Modules>
					<Recovery>
						<!--  
						If the packet/frame queue is exceed for a certain period of time(millisecond, ms), it will be automatically deleted. 
						If this value is set to zero, the stream will not be deleted. 
						
						-->
						<DeleteLazyStreamTimeout>10000</DeleteLazyStreamTimeout>
					</Recovery>
				</Modules>
		*/
		auto delete_lazy_stream_timeout_conf = _server_config->GetModules().GetRecovery().GetDeleteLazyStreamTimeout();
		if (delete_lazy_stream_timeout_conf > 0)
		{
			if (info.GetThresholdExceededTimeInUs() > delete_lazy_stream_timeout_conf)
			{
				ov::String vhost_app_name = info::ManagedQueue::ParseVHostApp(info.GetUrn().CStr());
				ov::String stream_name = info::ManagedQueue::ParseStream(info.GetUrn().CStr());

				if( !vhost_app_name.IsEmpty() && !stream_name.IsEmpty())
				{
					logtc("The %s queue has been exceeded for %lld ms. stream will be forcibly deleted. VhostApp(%s), Stream(%s)", info.GetUrn().CStr(), info.GetThresholdExceededTimeInUs(), vhost_app_name.CStr(), stream_name.CStr());
					auto vhost_app = info::VHostAppName(vhost_app_name);

					if(vhost_app.IsValid())
					{
						ocst::Orchestrator::GetInstance()->TerminateStream(vhost_app, stream_name);
					}

					// Clear memory fragmentation
					malloc_trim(0);
				}
			}
		}
	}

	std::map<uint32_t, std::shared_ptr<QueueMetrics>> ServerMetrics::GetQueueMetricsList()
	{
		std::shared_lock<std::shared_mutex> lock(_map_guard);

		return _queues;
	}
}  // namespace mon