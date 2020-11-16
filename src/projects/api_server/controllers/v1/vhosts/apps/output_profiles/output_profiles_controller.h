//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../../../controller.h"

namespace api
{
	namespace v1
	{
		class OutputProfilesController : public Controller<OutputProfilesController>
		{
		public:
			void PrepareHandlers() override;

		protected:
			// POST /v1/vhosts/<vhost_name>/apps/<app_name>/outputProfiles
			ApiResponse OnPostOutputProfile(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
											const std::shared_ptr<mon::HostMetrics> &vhost,
											const std::shared_ptr<mon::ApplicationMetrics> &app);

			// GET /v1/vhosts/<vhost_name>/apps/<app_name>/outputProfiles
			ApiResponse OnGetOutputProfileList(const std::shared_ptr<HttpClient> &client,
											   const std::shared_ptr<mon::HostMetrics> &vhost,
											   const std::shared_ptr<mon::ApplicationMetrics> &app);

			// GET /v1/vhosts/<vhost_name>/apps/<app_name>/outputProfiles/<output_profile_name>
			ApiResponse OnGetOutputProfile(const std::shared_ptr<HttpClient> &client,
										   const std::shared_ptr<mon::HostMetrics> &vhost,
										   const std::shared_ptr<mon::ApplicationMetrics> &app);

			// PUT /v1/vhosts/<vhost_name>/apps/<app_name>/outputProfiles/<output_profile_name>
			ApiResponse OnPutOutputProfile(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
										   const std::shared_ptr<mon::HostMetrics> &vhost,
										   const std::shared_ptr<mon::ApplicationMetrics> &app);

			// DELETE /v1/vhosts/<vhost_name>/apps/<app_name>/outputProfiles/<output_profile_name>
			ApiResponse OnDeleteOutputProfile(const std::shared_ptr<HttpClient> &client,
											  const std::shared_ptr<mon::HostMetrics> &vhost,
											  const std::shared_ptr<mon::ApplicationMetrics> &app);
		};
	}  // namespace v1
}  // namespace api
