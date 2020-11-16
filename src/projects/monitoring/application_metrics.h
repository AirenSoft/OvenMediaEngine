//
// Created by getroot on 20. 1. 16.
//
#pragma once

#include "base/common_types.h"
#include "base/info/info.h"
#include "common_metrics.h"
#include "stream_metrics.h"
#include <shared_mutex>

namespace mon
{
	class HostMetrics;
	class ApplicationMetrics : public info::Application, public CommonMetrics, public ov::EnableSharedFromThis<ApplicationMetrics>
	{
	const char* GetApplicationTypeName() final
	{
		return "ApplicationMetrics";
	}

	public:
		ApplicationMetrics(const std::shared_ptr<HostMetrics> &host_metrics, const info::Application &app_info)
			: info::Application(app_info),
			  _host_metrics(host_metrics)
		{
		}

		~ApplicationMetrics()
		{
			_host_metrics.reset();
			_streams.clear();
		}

		void Release()
		{
			_streams.clear();
		}

		std::shared_ptr<HostMetrics> GetHostMetrics()
		{
			return _host_metrics;
		}

		ov::String GetInfoString(bool show_children=true);
		void ShowInfo(bool show_children=true);

		bool OnStreamCreated(const info::Stream &stream);
		bool OnStreamDeleted(const info::Stream &stream);

		std::map<uint32_t, std::shared_ptr<StreamMetrics>> GetStreamMetricsList();

		std::shared_ptr<StreamMetrics> GetStreamMetrics(const info::Stream &stream);

		// Overriding from CommonMetrics 
		void IncreaseBytesIn(uint64_t value) override;
		void IncreaseBytesOut(PublisherType type, uint64_t value) override;
		void OnSessionConnected(PublisherType type) override;
		void OnSessionDisconnected(PublisherType type) override;

	private:
		std::shared_ptr<HostMetrics> _host_metrics;
		std::shared_mutex _map_guard;
		std::map<uint32_t, std::shared_ptr<StreamMetrics>> _streams;
	};
}  // namespace mon