//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./controller.h"

#include <main/main.h>

#include "./stats/stats_controller.h"
#include "./vhosts/vhosts_controller.h"

namespace api
{
	namespace v1
	{
		void Controller::PrepareHandlers()
		{
			RegisterGet(R"(\/version)", &Controller::OnGetVersion);

			CreateSubController<VHostsController>(R"(\/vhosts)");
			CreateSubController<stats::StatsController>(R"(\/stats)");
		};

		ApiResponse Controller::OnGetVersion(const std::shared_ptr<http::svr::HttpExchange> &client)
		{
			Json::Value response_value(Json::ValueType::objectValue);
			response_value["version"]	 = OME_VERSION;
			response_value["gitVersion"] = OME_GIT_VERSION;
			return ApiResponse(response_value);
		}
	}  // namespace v1
}  // namespace api
