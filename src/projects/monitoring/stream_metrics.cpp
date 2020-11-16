//
// Created by getroot on 20. 1. 16.
//

#include "stream_metrics.h"
#include "application_metrics.h"
#include "monitoring_private.h"

namespace mon
{
	ov::String StreamMetrics::GetInfoString()
	{
		ov::String out_str;

		out_str.Append(info::Stream::GetInfoString());

		if(GetSourceType() == StreamSourceType::Ovt || GetSourceType() == StreamSourceType::RtspPull)
		{
			out_str.AppendFormat("\n\tElapsed time to connect to origin server : %f ms\n"
									"\tElapsed time in response from origin server : %f ms\n",
									GetOriginRequestTimeMSec(), GetOriginResponseTimeMSec());
		}
		out_str.Append("\n");
		out_str.Append(CommonMetrics::GetInfoString());

		return out_str;
	}

	void StreamMetrics::ShowInfo()
	{
		logti("%s", GetInfoString().CStr());
	}

	// Getter
	int64_t StreamMetrics::GetOriginRequestTimeMSec() const
	{
		return _request_time_to_origin_msec;
	}
	int64_t StreamMetrics::GetOriginResponseTimeMSec() const
	{
		return _response_time_from_origin_msec;
	}

	// Setter
	void StreamMetrics::SetOriginRequestTimeMSec(int64_t value)
	{
		_request_time_to_origin_msec = value;
		UpdateDate();
	}
	void StreamMetrics::SetOriginResponseTimeMSec(int64_t value)
	{
		_response_time_from_origin_msec = value;
		UpdateDate();
	}

	void StreamMetrics::IncreaseBytesIn(uint64_t value)
	{
		CommonMetrics::IncreaseBytesIn(value);

		// If this stream is child then send event to parent
		auto origin_stream_info = GetOriginStream();
		if(origin_stream_info != nullptr)
		{
			auto origin_stream_metric = _app_metrics->GetStreamMetrics(*origin_stream_info);
			if(origin_stream_metric != nullptr)
			{
				origin_stream_metric->IncreaseBytesIn(value);
			}
		}
		else
		{
			// Forward value to AppMetrics to sum
			GetApplicationMetrics()->IncreaseBytesIn(value);
		}
	}

	void StreamMetrics::IncreaseBytesOut(PublisherType type, uint64_t value) 
	{
		CommonMetrics::IncreaseBytesOut(type, value);

		// If this stream is child then send event to parent
		auto origin_stream_info = GetOriginStream();
		if(origin_stream_info != nullptr)
		{
			auto origin_stream_metric = _app_metrics->GetStreamMetrics(*origin_stream_info);
			if(origin_stream_metric != nullptr)
			{
				origin_stream_metric->IncreaseBytesOut(type, value);
			}
		}
		else
		{
			// Forward value to AppMetrics to sum
			GetApplicationMetrics()->IncreaseBytesOut(type, value);
		}
	}

	void StreamMetrics::OnSessionConnected(PublisherType type)
	{
		CommonMetrics::OnSessionConnected(type);

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

			logti("A new session has started playing %s/%s on the %s publihser. %s(%u)/Stream total(%u)/App total(%u)", 
					GetApplicationInfo().GetName().CStr(), GetName().CStr(), 
					::StringFromPublisherType(type).CStr(), ::StringFromPublisherType(type).CStr(), GetConnections(type), GetTotalConnections(), GetApplicationMetrics()->GetTotalConnections());
		}
	}
	
	void StreamMetrics::OnSessionDisconnected(PublisherType type)
	{
		CommonMetrics::OnSessionDisconnected(type);

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

			logti("A session has been stopped playing %s/%s on the %s publihser. Concurrent Viewers[%s(%u)/Stream total(%u)/App total(%u)]", 
					GetApplicationInfo().GetName().CStr(), GetName().CStr(), 
					::StringFromPublisherType(type).CStr(), ::StringFromPublisherType(type).CStr(), GetConnections(type), GetTotalConnections(), GetApplicationMetrics()->GetTotalConnections());
		}
	}

}  // namespace mon