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
		
		ov::String out_str;
		out_str.AppendFormat("\tElapsed time to connect to origin server : %f ms\n"
								"\tElapsed time to request from origin server : %f ms\n",
								GetOriginRequestTimeMSec(), GetOriginResponseTimeMSec());

		logti("%s", out_str.CStr());

		CommonMetrics::ShowInfo();
	}

	// Getter
	double StreamMetrics::GetOriginRequestTimeMSec()
	{
		return _request_time_to_origin_msec;
	}
	double StreamMetrics::GetOriginResponseTimeMSec()
	{
		return _response_time_from_origin_msec;
	}

	// Setter
	void StreamMetrics::SetOriginRequestTimeMSec(double value)
	{
		_request_time_to_origin_msec = value;
		UpdateDate();
	}
	void StreamMetrics::SetOriginResponseTimeMSet(double value)
	{
		_response_time_from_origin_msec = value;
		UpdateDate();
	}

	void StreamMetrics::IncreaseBytesIn(uint64_t value)
	{
		// Forward value to AppMetrics to sum
		GetApplicationMetrics()->IncreaseBytesIn(value);
		CommonMetrics::IncreaseBytesIn(value);
	}

	void StreamMetrics::IncreaseBytesOut(PublisherType type, uint64_t value) 
	{
		// Forward value to AppMetrics to sum
		GetApplicationMetrics()->IncreaseBytesOut(type, value);
		CommonMetrics::IncreaseBytesOut(type, value);
	}

	void StreamMetrics::OnSessionConnected(PublisherType type)
	{
		// If this stream is child then send event to parent
		auto origin_stream_info = GetOriginStream();
		if(origin_stream_info != nullptr)
		{
			auto origin_stream_metric = _app_metrics->GetStreamMetrics(*origin_stream_info);
			if(origin_stream_metric != nullptr)
			{
				origin_stream_metric->OnSessionConnected(type);
			}
		}
		else
		{
			// Sending a connection event to application only if it hasn't origin stream to prevent double sum. 
			GetApplicationMetrics()->OnSessionConnected(type);
		}

		CommonMetrics::OnSessionConnected(type);
	}
	
	void StreamMetrics::OnSessionDisconnected(PublisherType type)
	{
		// If this stream is child then send event to parent
		auto origin_stream_info = GetOriginStream();
		if(origin_stream_info != nullptr)
		{
			auto origin_stream_metric = _app_metrics->GetStreamMetrics(*origin_stream_info);
			if(origin_stream_metric != nullptr)
			{
				origin_stream_metric->OnSessionDisconnected(type);
			}
		}
		else
		{
			// Sending a connection event to application only if it hasn't origin stream to prevent double sum. 
			GetApplicationMetrics()->OnSessionDisconnected(type);
		}

		CommonMetrics::OnSessionDisconnected(type);
	}

}  // namespace mon