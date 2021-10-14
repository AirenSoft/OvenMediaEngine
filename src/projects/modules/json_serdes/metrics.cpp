//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "application.h"
#include "common.h"
namespace serdes
{
	Json::Value JsonFromMetrics(const std::shared_ptr<const mon::CommonMetrics> &metrics)
	{
		if (metrics == nullptr)
		{
			return Json::nullValue;
		}

		Json::Value value;

		SetTimestamp(value, "createdTime", metrics->GetCreatedTime());
		SetTimestamp(value, "lastUpdatedTime", metrics->GetLastUpdatedTime());
		SetInt64(value, "totalBytesIn", metrics->GetTotalBytesIn());
		SetInt64(value, "totalBytesOut", metrics->GetTotalBytesOut());
		SetTimestamp(value, "lastRecvTime", metrics->GetLastRecvTime());
		SetTimestamp(value, "lastSentTime", metrics->GetLastSentTime());
		SetInt(value, "totalConnections", metrics->GetTotalConnections());
		SetInt(value, "maxTotalConnections", metrics->GetMaxTotalConnections());
		SetTimestamp(value, "maxTotalConnectionTime", metrics->GetMaxTotalConnectionsTime());

		Json::Value &connections = value["connections"];
		SetInt(connections, ov::String::FormatString("%s", StringFromPublisherType(PublisherType::Webrtc).LowerCaseString().CStr()).CStr(), metrics->GetConnections(PublisherType::Webrtc));
		SetInt(connections, ov::String::FormatString("%s", StringFromPublisherType(PublisherType::LlDash).LowerCaseString().CStr()).CStr(), metrics->GetConnections(PublisherType::LlDash));
		SetInt(connections, ov::String::FormatString("%s", StringFromPublisherType(PublisherType::Hls).LowerCaseString().CStr()).CStr(), metrics->GetConnections(PublisherType::Hls));
		SetInt(connections, ov::String::FormatString("%s", StringFromPublisherType(PublisherType::Dash).LowerCaseString().CStr()).CStr(), metrics->GetConnections(PublisherType::Dash));
		SetInt(connections, ov::String::FormatString("%s", StringFromPublisherType(PublisherType::Ovt).LowerCaseString().CStr()).CStr(), metrics->GetConnections(PublisherType::Ovt));

		return value;
	}

	Json::Value JsonFromStreamMetrics(const std::shared_ptr<const mon::StreamMetrics> &metrics)
	{
		Json::Value value = JsonFromMetrics(metrics);

		if (value.isNull())
		{
			return value;
		}

		SetTimeInterval(value, "requestTimeToOrigin", metrics->GetOriginConnectionTimeMSec());
		SetTimeInterval(value, "responseTimeFromOrigin", metrics->GetOriginSubscribeTimeMSec());

		return value;
	}
}  // namespace serdes