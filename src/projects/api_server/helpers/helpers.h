//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/http/http_datastructure.h>
#include <monitoring/monitoring.h>

namespace api
{
	std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> GetVirtualHostList();
	std::shared_ptr<mon::HostMetrics> GetVirtualHost(const ov::String &vhost_name);

	std::map<uint32_t, std::shared_ptr<mon::ApplicationMetrics>> GetApplicationList(const std::shared_ptr<mon::HostMetrics> &vhost);
	std::shared_ptr<mon::ApplicationMetrics> GetApplication(const std::shared_ptr<mon::HostMetrics> &vhost, const ov::String &app_name);

	std::map<uint32_t, std::shared_ptr<mon::StreamMetrics>> GetStreamList(const std::shared_ptr<mon::ApplicationMetrics> &application);
	std::shared_ptr<mon::StreamMetrics> GetStream(const std::shared_ptr<mon::ApplicationMetrics> &application, const ov::String &stream_name, std::vector<std::shared_ptr<mon::StreamMetrics>> *output_streams);

	std::shared_ptr<const http::HttpError> GetRequestBody(
		const std::shared_ptr<http::svr::HttpConnection> &client,
		Json::Value *request_body);

	std::shared_ptr<const http::HttpError> GetVirtualHostMetrics(
		const ov::MatchResult &match_result,
		std::shared_ptr<mon::HostMetrics> *vhost_metrics);

	std::shared_ptr<const http::HttpError> GetApplicationMetrics(
		const ov::MatchResult &match_result,
		const std::shared_ptr<mon::HostMetrics> &vhost_metrics,
		std::shared_ptr<mon::ApplicationMetrics> *app_metrics);

	std::shared_ptr<const http::HttpError> GetStreamMetrics(
		const ov::MatchResult &match_result,
		const std::shared_ptr<mon::HostMetrics> &vhost_metrics,
		const std::shared_ptr<mon::ApplicationMetrics> &app_metrics,
		std::shared_ptr<mon::StreamMetrics> *stream_metrics,
		std::vector<std::shared_ptr<mon::StreamMetrics>> *output_streams);

	class MultipleStatus
	{
	public:
		void AddStatusCode(http::StatusCode status_code);
		void AddStatusCode(const std::shared_ptr<const ov::Error> &error);
		http::StatusCode GetStatusCode() const;

		bool HasOK() const
		{
			return _has_ok;
		}

	protected:
		int _count = 0;
		http::StatusCode _last_status_code = http::StatusCode::OK;

		bool _has_ok = true;
	};
}  // namespace api