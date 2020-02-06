//
// Created by getroot on 20. 1. 16.
//

#include "stream_metrics.h"
#include "application_metrics.h"
#include "monitoring_private.h"

namespace mon
{
	void StreamMetrics::ShowInfo()
	{
		info::Stream::ShowInfo();

		ov::String out_str = ov::String::FormatString("\n------------ Statistics ------------\n");
		out_str.AppendFormat(
			"\tLast update time : %s\n"
			"\tElapsed time to request to origin server : %d ms\n"
			"\tElapsed time to response from origin server : %d ms\n",
			ov::Converter::ToString(GetLastUpdatedTime()).CStr(),
			GetOriginRequestTimeMSec(), GetOriginResponseTimeMSec());

		out_str.AppendFormat(
			"\tTotal bytes in : %u\n"
			"\tTotal bytes out : %u\n"
			"\tConcurrent connections : %u\n"
			"\tMax total connections : %u\n",
			GetTotalBytesIn(), GetTotalBytesOut(), GetTotalConnections(), GetMaxTotalConnections());

		out_str.AppendFormat("\n\t\t[Publisher Info]\n");
		for (int i = 0; i < static_cast<int8_t>(PublisherType::NumberOfPublishers); i++)
		{
			out_str.AppendFormat("\t\t >> %s : Bytes out(%u) Concurrent connections (%u)\n",
								 ov::Converter::ToString(static_cast<PublisherType>(i)).CStr(),
								 GetBytesOut(static_cast<PublisherType>(i)),
								 GetConnections(static_cast<PublisherType>(i)));
		}

        logti("%s", out_str.CStr());
	}

	// Getter
	uint32_t StreamMetrics::GetOriginRequestTimeMSec()
	{
		return _request_time_to_origin_msec;
	}
	uint32_t StreamMetrics::GetOriginResponseTimeMSec()
	{
		return _response_time_from_origin_msec;
	}

	uint64_t StreamMetrics::GetTotalBytesIn()
	{
		return _total_bytes_in.load();
	}
	uint64_t StreamMetrics::GetTotalBytesOut()
	{
		return _total_bytes_out;
	}
	uint32_t StreamMetrics::GetTotalConnections()
	{
		return _total_connections;
	}

	uint32_t StreamMetrics::GetMaxTotalConnections()
	{
		return _max_total_connections;
	}
	std::chrono::system_clock::time_point StreamMetrics::GetMaxTotalConnectionsTime()
	{
		return _max_total_connection_time;
	}

	uint64_t StreamMetrics::GetBytesOut(PublisherType type)
	{
		return _publisher_metrics[static_cast<int8_t>(type)]._bytes_out;
	}
	uint64_t StreamMetrics::GetConnections(PublisherType type)
	{
		return _publisher_metrics[static_cast<int8_t>(type)]._connections;
	}

	// Setter
	void StreamMetrics::SetOriginRequestTimeMSec(uint32_t value)
	{
		_request_time_to_origin_msec = value;
		UpdateDate();
	}
	void StreamMetrics::SetOriginResponseTimeMSet(uint32_t value)
	{
		_response_time_from_origin_msec = value;
		UpdateDate();
	}

	void StreamMetrics::IncreaseBytesIn(uint64_t value)
	{
		_total_bytes_in += value;
		UpdateDate();
	}
	void StreamMetrics::IncreaseBytesOut(PublisherType type, uint64_t value)
	{
		_publisher_metrics[static_cast<int8_t>(type)]._bytes_out += value;
		_total_bytes_out += value;
		UpdateDate();
	}

	void StreamMetrics::OnSessionConnected(PublisherType type)
	{
		_publisher_metrics[static_cast<int8_t>(type)]._connections++;
		_total_connections++;
		if (_total_connections > _max_total_connections)
		{
			_max_total_connections.exchange(_total_connections);
			_max_total_connection_time = std::chrono::system_clock::now();
		}
		UpdateDate();
	}
	void StreamMetrics::OnSessionDisconnected(PublisherType type)
	{
		_publisher_metrics[static_cast<int8_t>(type)]._connections--;
		_total_connections--;
		UpdateDate();
	}
}  // namespace mon