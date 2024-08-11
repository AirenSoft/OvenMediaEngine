//
// Created by getroot on 20. 1. 16.
//

#include "stream_metrics.h"
#include "application_metrics.h"
#include "monitoring_private.h"

namespace mon
{
	ov::String StreamMetrics::GetInfoString([[maybe_unused]] bool show_children)
	{
		ov::String out_str;

		out_str.Append(info::Stream::GetInfoString());

		if(GetSourceType() == StreamSourceType::Ovt || GetSourceType() == StreamSourceType::RtspPull)
		{
			out_str.AppendFormat("\n\tElapsed time to connect to origin server : %llu ms\n"
									"\tElapsed time to subscribe to origin server : %llu ms\n",
									GetOriginConnectionTimeMSec(), GetOriginSubscribeTimeMSec());
		}
		out_str.Append("\n");
		out_str.Append(CommonMetrics::GetInfoString());

		return out_str;
	}

	void StreamMetrics::ShowInfo([[maybe_unused]] bool show_children)
	{
		logti("%s", GetInfoString(show_children).CStr());
	}

	void StreamMetrics::LinkOutputStreamMetrics(const std::shared_ptr<StreamMetrics> &stream)
	{
		_output_stream_metrics.push_back(stream);
	}
	
	std::vector<std::shared_ptr<StreamMetrics>> StreamMetrics::GetLinkedOutputStreamMetrics() const
	{
		return _output_stream_metrics;
	}

	// Getter
	int64_t StreamMetrics::GetOriginConnectionTimeMSec() const
	{
		return _connection_time_to_origin_msec.load();
	}
	int64_t StreamMetrics::GetOriginSubscribeTimeMSec() const
	{
		return _subscribe_time_from_origin_msec.load();
	}

	// Setter
	void StreamMetrics::SetOriginConnectionTimeMSec(int64_t value)
	{
		_connection_time_to_origin_msec = value;
		UpdateDate();
	}
	void StreamMetrics::SetOriginSubscribeTimeMSec(int64_t value)
	{
		_subscribe_time_from_origin_msec = value;
		UpdateDate();
	}

	void StreamMetrics::IncreaseBytesIn(uint64_t value)
	{
		CommonMetrics::IncreaseBytesIn(value);

		// If this stream is child then send event to parent
		auto origin_stream_info = GetLinkedInputStream();
		if(origin_stream_info != nullptr)
		{
			auto origin_stream_metric = _app_metrics->GetStreamMetrics(*origin_stream_info);
			if(origin_stream_metric != nullptr)
			{
				origin_stream_metric->IncreaseBytesIn(value);
			}
		}
	}

	void StreamMetrics::IncreaseBytesOut(PublisherType type, uint64_t value) 
	{
		CommonMetrics::IncreaseBytesOut(type, value);

		// If this stream is child then send event to parent
		auto origin_stream_info = GetLinkedInputStream();
		if(origin_stream_info != nullptr)
		{
			auto origin_stream_metric = _app_metrics->GetStreamMetrics(*origin_stream_info);
			if(origin_stream_metric != nullptr)
			{
				origin_stream_metric->IncreaseBytesOut(type, value);
			}
		}
	}

	void StreamMetrics::OnSessionConnected(PublisherType type)
	{
		CommonMetrics::OnSessionConnected(type);

		// If this stream is child then send event to parent
		auto origin_stream_info = GetLinkedInputStream();
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
			logti("A new session has started playing %s/%s on the %s publisher. %s(%u)/Stream total(%u)/App total(%u)", 
					GetApplicationInfo().GetVHostAppName().CStr(), GetName().CStr(), 
					::StringFromPublisherType(type).CStr(), ::StringFromPublisherType(type).CStr(), GetConnections(type), GetTotalConnections(), GetApplicationMetrics()->GetTotalConnections());
		}
	}
	
	void StreamMetrics::OnSessionDisconnected(PublisherType type)
	{
		CommonMetrics::OnSessionDisconnected(type);

		// If this stream is child then send event to parent
		auto origin_stream_info = GetLinkedInputStream();
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
			logti("A session has been stopped playing %s/%s on the %s publisher. Concurrent Viewers[%s(%u)/Stream total(%u)/App total(%u)]", 
					GetApplicationInfo().GetVHostAppName().CStr(), GetName().CStr(), 
					::StringFromPublisherType(type).CStr(), ::StringFromPublisherType(type).CStr(), GetConnections(type), GetTotalConnections(), GetApplicationMetrics()->GetTotalConnections());
		}
	}

	void StreamMetrics::OnSessionsDisconnected(PublisherType type, uint64_t number_of_sessions)
	{
		if (number_of_sessions == 0)
		{
			return ;
		}

		CommonMetrics::OnSessionsDisconnected(type, number_of_sessions);

		// If this stream is child then send event to parent
		auto origin_stream_info = GetLinkedInputStream();
		if(origin_stream_info != nullptr)
		{
			auto origin_stream_metric = _app_metrics->GetStreamMetrics(*origin_stream_info);
			if(origin_stream_metric != nullptr)
			{
				origin_stream_metric->OnSessionsDisconnected(type, number_of_sessions);
			}
		}
		else
		{
			logti("%u sessions has been stopped playing %s/%s on the %s publisher. Concurrent Viewers[%s(%u)/Stream total(%u)/App total(%u)]", 
					number_of_sessions,
					GetApplicationInfo().GetVHostAppName().CStr(), GetName().CStr(), 
					::StringFromPublisherType(type).CStr(), ::StringFromPublisherType(type).CStr(), GetConnections(type), GetTotalConnections(), GetApplicationMetrics()->GetTotalConnections());
		}
	}

}  // namespace mon