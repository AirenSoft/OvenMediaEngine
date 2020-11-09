//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../../controller.h"

namespace api
{
	namespace v1
	{
		class AppActionsController : public Controller<AppActionsController>
		{
		public:
			void PrepareHandlers() override;

		protected:
			ApiResponse OnGetRecords(const std::shared_ptr<HttpClient> &client,
									 const std::shared_ptr<mon::HostMetrics> &vhost,
									 const std::shared_ptr<mon::ApplicationMetrics> &app);
			ApiResponse OnPostStartRecord(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
										  const std::shared_ptr<mon::HostMetrics> &vhost,
										  const std::shared_ptr<mon::ApplicationMetrics> &app);
			ApiResponse OnPostStopRecord(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
										 const std::shared_ptr<mon::HostMetrics> &vhost,
										 const std::shared_ptr<mon::ApplicationMetrics> &app);

			ApiResponse OnGetPushes(const std::shared_ptr<HttpClient> &client);
			ApiResponse OnPostStartPush(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body);
			ApiResponse OnPostStopPush(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body);

			// POST /v1/vhosts/<vhost_name>/apps/<app_name>:<action>
			ApiResponse OnPostDummyAction(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
										  const std::shared_ptr<mon::HostMetrics> &vhost,
										  const std::shared_ptr<mon::ApplicationMetrics> &app);

			// GET /v1/vhosts/<vhost_name>/apps/<app_name>:<action>
			ApiResponse OnGetDummyAction(const std::shared_ptr<HttpClient> &client,
										 const std::shared_ptr<mon::HostMetrics> &vhost,
										 const std::shared_ptr<mon::ApplicationMetrics> &app);
		};
	}  // namespace v1
}  // namespace api
