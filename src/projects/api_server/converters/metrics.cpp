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

namespace api
{
	namespace conv
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
			SetInt(value, "totalConnections", metrics->GetTotalConnections());
			SetInt(value, "maxTotalConnections", metrics->GetMaxTotalConnections());
			SetTimestamp(value, "maxTotalConnectionTime", metrics->GetMaxTotalConnectionsTime());
			SetTimestamp(value, "lastRecvTime", metrics->GetLastRecvTime());
			SetTimestamp(value, "lastSentTime", metrics->GetLastSentTime());

			return std::move(value);
		}

		Json::Value JsonFromStreamMetrics(const std::shared_ptr<const mon::StreamMetrics> &metrics)
		{
			Json::Value value = JsonFromMetrics(metrics);

			if (value.isNull())
			{
				return std::move(value);
			}

			SetTimeInterval(value, "requestTimeToOrigin", metrics->GetOriginRequestTimeMSec());
			SetTimeInterval(value, "responseTimeFromOrigin", metrics->GetOriginResponseTimeMSec());

			return std::move(value);
		}
	}  // namespace conv
}  // namespace api