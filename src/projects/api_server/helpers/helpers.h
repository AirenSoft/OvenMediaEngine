//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <monitoring/monitoring.h>
#include <orchestrator/orchestrator.h>

#include "./multiple_status.h"

namespace api
{
	std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> GetVirtualHostList();
	std::shared_ptr<mon::HostMetrics> GetVirtualHost(const ov::String &vhost_name);

	std::map<uint32_t, std::shared_ptr<mon::ApplicationMetrics>> GetApplicationList(const std::shared_ptr<mon::HostMetrics> &vhost);
	std::shared_ptr<mon::ApplicationMetrics> GetApplication(const std::shared_ptr<mon::HostMetrics> &vhost, const ov::String &app_name);

	std::map<uint32_t, std::shared_ptr<mon::StreamMetrics>> GetStreamList(const std::shared_ptr<mon::ApplicationMetrics> &application);
	std::shared_ptr<mon::StreamMetrics> GetStream(const std::shared_ptr<mon::ApplicationMetrics> &application, const ov::String &stream_name, std::vector<std::shared_ptr<mon::StreamMetrics>> *output_streams);

	MAY_THROWS(HttpError)
	void GetRequestBody(
		const std::shared_ptr<http::svr::HttpConnection> &client,
		Json::Value *request_body);

	MAY_THROWS(HttpError)
	void GetVirtualHostMetrics(
		const ov::MatchResult &match_result,
		std::shared_ptr<mon::HostMetrics> *vhost_metrics);

	MAY_THROWS(HttpError)
	void GetApplicationMetrics(
		const ov::MatchResult &match_result,
		const std::shared_ptr<mon::HostMetrics> &vhost_metrics,
		std::shared_ptr<mon::ApplicationMetrics> *app_metrics);

	MAY_THROWS(HttpError)
	void GetStreamMetrics(
		const ov::MatchResult &match_result,
		const std::shared_ptr<mon::HostMetrics> &vhost_metrics,
		const std::shared_ptr<mon::ApplicationMetrics> &app_metrics,
		std::shared_ptr<mon::StreamMetrics> *stream_metrics,
		std::vector<std::shared_ptr<mon::StreamMetrics>> *output_streams);

	void FillDefaultAppConfigValues(Json::Value &app_config);

	void ThrowIfVirtualIsReadOnly(const cfg::vhost::VirtualHost &vhost_config);
	void ThrowIfOrchestratorNotSucceeded(ocst::Result result, const char *action, const char *resource_name, const char *resource_path);

}  // namespace api