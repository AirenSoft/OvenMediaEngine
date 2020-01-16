//
// Created by getroot on 20. 1. 16.
//

#pragma once

#include "base/common_types.h"
#include "base/info/info.h"
#include "base/info/stream.h"
#include "monitoring_common_types.h"

namespace mon
{
	class StreamMetrics : public info::Stream
	{

	private:
		// Related to origin
		uint32_t _origin_request_time_msec;
		uint32_t _origin_response_time_msec;

		// TODO: If the source type is LIVE_TRANSCODER, what can we provide some metrics

		// Total
		uint64_t _bytes_in;
		uint64_t _bytes_out;
		uint32_t _total_connections;

		class PublisherMetrics
		{
		public:
			uint64_t _bytes_in;
			uint64_t _bytes_out;
			uint32_t _total_connections;
		} publisher_metrics[PublisherType::NUMBER_OF_PUBLISHERS];
	};
}