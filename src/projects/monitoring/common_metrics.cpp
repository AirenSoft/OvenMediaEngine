//
// Created by getroot on 20. 1. 31.
//
#include "common_metrics.h"
#include "monitoring_private.h"

namespace mon
{
    CommonMetrics::CommonMetrics()
    {
        _total_bytes_in = 0;
        _total_bytes_out = 0;
        _total_connections = 0;
		_max_total_connections = 0;

        _max_total_connection_time = std::chrono::system_clock::now();
        _last_sent_time = std::chrono::system_clock::now();

        for(int i=0; i<static_cast<int8_t>(PublisherType::NumberOfPublishers); i++)
        {
            _publisher_metrics[i]._bytes_out = 0;
            _publisher_metrics[i]._connections = 0;
        }
        _created_time = std::chrono::system_clock::now();
        UpdateDate();
    }

	void CommonMetrics::ShowInfo()
	{
		ov::String out_str;

		out_str.AppendFormat(
			"\tLast update time : %s, tLast sent time : %s\n",
			ov::Converter::ToString(GetLastUpdatedTime()).CStr(),
			ov::Converter::ToString(GetLastSentTime()).CStr());

		out_str.AppendFormat(
			"\t[Total stats]\n"
			"\tBytes in : %u, Bytes out : %u, Concurrent connections : %u, Max connections : %u\n",
			GetTotalBytesIn(), GetTotalBytesOut(), GetTotalConnections(), GetMaxTotalConnections());

		out_str.AppendFormat("\n\t\t[By publisher]\n");
		for (int i = 0; i < static_cast<int8_t>(PublisherType::NumberOfPublishers); i++)
		{
			out_str.AppendFormat("\t\t >> %s : Bytes out(%u) Concurrent connections (%u)\n",
								 ov::Converter::ToString(static_cast<PublisherType>(i)).CStr(),
								 GetBytesOut(static_cast<PublisherType>(i)),
								 GetConnections(static_cast<PublisherType>(i)));
		}

        logti("%s", out_str.CStr());
	}

    uint32_t CommonMetrics::GetUnusedTimeSec()
    {
        auto current = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(current - GetLastUpdatedTime()).count();
    }

    const std::chrono::system_clock::time_point& CommonMetrics::GetLastUpdatedTime()
    {
        return _last_updated_time;
    }

    uint64_t CommonMetrics::GetTotalBytesIn()
	{
		return _total_bytes_in.load();
	}
	uint64_t CommonMetrics::GetTotalBytesOut()
	{
		return _total_bytes_out;
	}
	uint32_t CommonMetrics::GetTotalConnections()
	{
		return _total_connections;
	}

	uint32_t CommonMetrics::GetMaxTotalConnections()
	{
		return _max_total_connections;
	}
	std::chrono::system_clock::time_point CommonMetrics::GetMaxTotalConnectionsTime()
	{
		return _max_total_connection_time;
	}

	std::chrono::system_clock::time_point CommonMetrics::GetLastSentTime()
	{
		return _last_sent_time;
	}

	uint64_t CommonMetrics::GetBytesOut(PublisherType type)
	{
		return _publisher_metrics[static_cast<int8_t>(type)]._bytes_out;
	}
	uint64_t CommonMetrics::GetConnections(PublisherType type)
	{
		return _publisher_metrics[static_cast<int8_t>(type)]._connections;
	}

    void CommonMetrics::IncreaseBytesIn(uint64_t value)
	{
		_total_bytes_in += value;
		UpdateDate();
	}
	void CommonMetrics::IncreaseBytesOut(PublisherType type, uint64_t value)
	{
		if(value == 0)
		{
			return;
		}
		
		_publisher_metrics[static_cast<int8_t>(type)]._bytes_out += value;
		_total_bytes_out += value;
		_last_sent_time = std::chrono::system_clock::now();
		UpdateDate();
	}

	void CommonMetrics::OnSessionConnected(PublisherType type)
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
	void CommonMetrics::OnSessionDisconnected(PublisherType type)
	{
		if(_publisher_metrics[static_cast<int8_t>(type)]._connections > 0)
		{
			_publisher_metrics[static_cast<int8_t>(type)]._connections--;
		}

		if(_total_connections > 0)
		{
			_total_connections--;
		}

		UpdateDate();
	}

    // Renew last updated time
    void CommonMetrics::UpdateDate()
    {
        _last_updated_time = std::chrono::system_clock::now();
    }
}