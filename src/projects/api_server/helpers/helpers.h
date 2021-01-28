//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/http_server/http_datastructure.h>
#include <monitoring/monitoring.h>

namespace api
{
	std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> GetVirtualHostList();
	std::shared_ptr<mon::HostMetrics> GetVirtualHost(const std::string_view &vhost_name);

	std::map<uint32_t, std::shared_ptr<mon::ApplicationMetrics>> GetApplicationList(const std::shared_ptr<mon::HostMetrics> &vhost);
	std::shared_ptr<mon::ApplicationMetrics> GetApplication(const std::shared_ptr<mon::HostMetrics> &vhost, const std::string_view &app_name);

	std::map<uint32_t, std::shared_ptr<mon::StreamMetrics>> GetStreamList(const std::shared_ptr<mon::ApplicationMetrics> &application);
	std::shared_ptr<mon::StreamMetrics> GetStream(const std::shared_ptr<mon::ApplicationMetrics> &application, const std::string_view &stream_name, std::vector<std::shared_ptr<mon::StreamMetrics>> *output_streams);

	class MultipleStatus
	{
	public:
		void AddStatusCode(HttpStatusCode status_code);
		void AddStatusCode(const std::shared_ptr<const ov::Error> &error);
		HttpStatusCode GetStatusCode() const;

		bool HasOK() const
		{
			return _has_ok;
		}

	protected:
		int _count = 0;
		HttpStatusCode _last_status_code = HttpStatusCode::OK;

		bool _has_ok = true;
	};
}  // namespace api