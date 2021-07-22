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

		Json::Value &session = value["sessions"];
		for (int i = 1; i < static_cast<int8_t>(PublisherType::NumberOfPublishers); i++)
		{
			SetInt(session, 
					ov::String::FormatString("%s", StringFromPublisherType(static_cast<PublisherType>(i)).LowerCaseString().CStr()).CStr(), 
					metrics->GetConnections(static_cast<PublisherType>(i)));
		}

		return value;
	}

	Json::Value JsonFromStreamMetrics(const std::shared_ptr<const mon::StreamMetrics> &metrics)
	{
		Json::Value value = JsonFromMetrics(metrics);

		if (value.isNull())
		{
			return value;
		}

		SetTimeInterval(value, "requestTimeToOrigin", metrics->GetOriginRequestTimeMSec());
		SetTimeInterval(value, "responseTimeFromOrigin", metrics->GetOriginResponseTimeMSec());

		return value;
	}
}  // namespace serdes