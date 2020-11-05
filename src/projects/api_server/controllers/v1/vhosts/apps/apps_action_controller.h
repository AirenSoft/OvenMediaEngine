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
		class AppsActionController : public Controller<AppsActionController>
		{
		public:
			void PrepareHandlers() override;

		protected:

			// POST /v1/vhosts/<vhost_name>/apps/<app_name>:<action>
			ApiResponse OnPostDummyAction(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body);

			// GET /v1/vhosts/<vhost_name>/apps/<app_name>:<action>
			ApiResponse OnGetDummyAction(const std::shared_ptr<HttpClient> &client);
		};
	}  // namespace v1
}  // namespace api
