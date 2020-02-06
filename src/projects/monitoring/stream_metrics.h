//
// Created by getroot on 20. 1. 16.
//

#pragma once

#include <base/ovlibrary/converter.h>
#include "base/common_types.h"
#include "base/info/info.h"
#include "base/info/stream.h"
#include "common_metrics.h"

namespace mon
{
	class ApplicationMetrics;
	class StreamMetrics : public info::Stream, public CommonMetrics
	{
	public:
		StreamMetrics(const std::shared_ptr<ApplicationMetrics> &app_metrics, const info::Stream &stream)
		: info::Stream(stream), 
            _app_metrics(app_metrics)
		{
		}
		std::shared_ptr<ApplicationMetrics> GetApplicationMetrics()
		{
			return _app_metrics;
		}

		void ShowInfo();

		// Getter
		uint32_t GetOriginRequestTimeMSec();
		uint32_t GetOriginResponseTimeMSec();

		uint64_t GetTotalBytesIn();
		uint64_t GetTotalBytesOut();
		uint32_t GetTotalConnections();
		uint32_t GetMaxTotalConnections();
		std::chrono::system_clock::time_point GetMaxTotalConnectionsTime();

		uint64_t GetBytesOut(PublisherType type);
		uint64_t GetConnections(PublisherType type);

		// Setter
		void SetOriginRequestTimeMSec(uint32_t value);
		void SetOriginResponseTimeMSet(uint32_t value);

		void IncreaseBytesIn(uint64_t value);
		void IncreaseBytesOut(PublisherType type, uint64_t value);
		
		void OnSessionConnected(PublisherType type);
		void OnSessionDisconnected(PublisherType type);

	private:
		// Related to origin, From Provider
		std::atomic<uint32_t> _request_time_to_origin_msec;
		std::atomic<uint32_t> _response_time_from_origin_msec;

		// TODO: If the source type is LIVE_TRANSCODER, what can we provide some more metrics

		// From Provider
		std::atomic<uint64_t> _total_bytes_in;

		// From Publishers
		std::atomic<uint64_t> _total_bytes_out;
		std::atomic<uint32_t> _total_connections;
		
		std::atomic<uint32_t> _max_total_connections;
		// Time to reach maximum number of connections. 
		// TODO(Getroot): Does it need mutex? Check!
		std::chrono::system_clock::time_point	_max_total_connection_time;

		// From Publishers
		class PublisherMetrics
		{
		public:
			std::atomic<uint64_t> _bytes_out;
			std::atomic<uint32_t> _connections;
		};

		std::shared_ptr<ApplicationMetrics>	_app_metrics;
		PublisherMetrics _publisher_metrics[static_cast<int8_t>(PublisherType::NumberOfPublishers)];
	};
}