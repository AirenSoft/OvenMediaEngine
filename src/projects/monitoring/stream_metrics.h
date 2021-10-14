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
			_connection_time_to_origin_msec = 0;
			_subscribe_time_from_origin_msec = 0;
			logd("DEBUG", "StreamMetric (%s / %s) Created", GetName().CStr(), GetUUID().CStr());
		}

		~StreamMetrics()
		{
			logd("DEBUG", "StreamMetric (%s / %s) Destroyed", GetName().CStr(), GetUUID().CStr());
			_app_metrics.reset();
		}

		std::shared_ptr<ApplicationMetrics> GetApplicationMetrics()
		{
			return _app_metrics;
		}

		ov::String GetInfoString();
		void ShowInfo() override;

		void LinkOutputStreamMetrics(const std::shared_ptr<StreamMetrics> &stream);
		std::vector<std::shared_ptr<StreamMetrics>> GetLinkedOutputStreamMetrics() const;

		int64_t GetOriginConnectionTimeMSec() const;
		int64_t GetOriginSubscribeTimeMSec() const;
		void SetOriginConnectionTimeMSec(int64_t value);
		void SetOriginSubscribeTimeMSec(int64_t value);

		// Overriding from CommonMetrics 
		void IncreaseBytesIn(uint64_t value) override;
		void IncreaseBytesOut(PublisherType type, uint64_t value) override;
		void OnSessionConnected(PublisherType type) override;
		void OnSessionDisconnected(PublisherType type) override;
	private:
		// Related to origin, From Provider
		std::atomic<int64_t> _connection_time_to_origin_msec = 0;
		std::atomic<int64_t> _subscribe_time_from_origin_msec = 0;

		// If this stream is from Provider(input stream) it has multiple output streams
		std::vector<std::shared_ptr<StreamMetrics>> _output_stream_metrics;

		std::shared_ptr<ApplicationMetrics>	_app_metrics;
	};
}