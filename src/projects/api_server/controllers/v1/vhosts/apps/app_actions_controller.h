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

			ApiResponse OnGetRecords(const std::shared_ptr<HttpClient> &clnt);
			ApiResponse OnPostStartRecord(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body);
			ApiResponse OnPostStopRecord(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body);

			ApiResponse OnGetPushes(const std::shared_ptr<HttpClient> &clnt);
			ApiResponse OnPostStartPush(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body);
			ApiResponse OnPostStopPush(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body);

			// POST /v1/vhosts/<vhost_name>/apps/<app_name>:<action>
			ApiResponse OnPostDummyAction(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body);

			// GET /v1/vhosts/<vhost_name>/apps/<app_name>:<action>
			ApiResponse OnGetDummyAction(const std::shared_ptr<HttpClient> &clnt);
		};
	}  // namespace v1
}  // namespace api
