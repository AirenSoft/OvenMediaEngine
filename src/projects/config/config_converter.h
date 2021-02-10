//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

// Forward declaration
class ReservedStreamMetrics;
namespace mon
{
	class ApplicationMetrics;
	class HostMetrics;
}  // namespace mon

namespace cfg
{
	class Server;

	namespace conv
	{
		Json::Value GetMpegTsStreamListFromMetrics(Json::Value providers, const std::map<uint32_t, std::shared_ptr<ReservedStreamMetrics>> &reserved_stream_list);
		Json::Value GetApplicationFromMetrics(const std::shared_ptr<const mon::ApplicationMetrics> &app_metrics);
		Json::Value GetApplicationListFromMetrics(const std::map<uint32_t, std::shared_ptr<mon::ApplicationMetrics>> &app_metrics_list, bool include_dynamic_app);
		Json::Value GetVirtualHostFromMetrics(const std::shared_ptr<mon::HostMetrics> &vhost_metrics, bool include_dynamic_app);
		Json::Value GetVirtualHostListFromMetrics(Json::Value server_config, const std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> &vhost_metrics_list, bool include_dynamic_app);
		Json::Value GetServerJsonFromConfig(const std::shared_ptr<Server> &server_config, bool include_dynamic_app = false);

	}  // namespace conv
}  // namespace cfg
