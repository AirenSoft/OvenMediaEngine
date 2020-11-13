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
			_request_time_to_origin_msec = 0;
			_response_time_from_origin_msec = 0;
		}

		~StreamMetrics()
		{
			_app_metrics.reset();
		}

		std::shared_ptr<ApplicationMetrics> GetApplicationMetrics()
		{
			return _app_metrics;
		}

		ov::String GetInfoString();
		void ShowInfo() override;

		int64_t GetOriginRequestTimeMSec() const;
		int64_t GetOriginResponseTimeMSec() const;
		void SetOriginRequestTimeMSec(int64_t value);
		void SetOriginResponseTimeMSec(int64_t value);

		// Overriding from CommonMetrics 
		void IncreaseBytesIn(uint64_t value) override;
		void IncreaseBytesOut(PublisherType type, uint64_t value) override;
		void OnSessionConnected(PublisherType type) override;
		void OnSessionDisconnected(PublisherType type) override;
	private:
		// Related to origin, From Provider
		std::atomic<int64_t> _request_time_to_origin_msec;
		std::atomic<int64_t> _response_time_from_origin_msec;

		std::shared_ptr<ApplicationMetrics>	_app_metrics;
	};
}