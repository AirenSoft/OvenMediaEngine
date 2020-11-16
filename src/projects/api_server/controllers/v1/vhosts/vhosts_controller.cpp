//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "vhosts_controller.h"

#include <config/config.h>

#include "../../../api_private.h"
#include "apps/apps_controller.h"

namespace api
{
	namespace v1
	{
		void VHostsController::PrepareHandlers()
		{
			RegisterGet(R"()", &VHostsController::OnGetVhostList);
			RegisterGet(R"(\/(?<vhost_name>[^\/]*))", &VHostsController::OnGetVhost);

			CreateSubController<AppsController>(R"(\/(?<vhost_name>[^\/]*)\/apps)");
		}

		ApiResponse VHostsController::OnGetVhostList(const std::shared_ptr<HttpClient> &client)
		{
			auto vhost_list = GetVirtualHostList();
			Json::Value response(Json::ValueType::arrayValue);

			for (const auto &item : vhost_list)
			{
				response.append(item.second->GetName().CStr());
			}

			return response;
		}

		ApiResponse VHostsController::OnGetVhost(const std::shared_ptr<HttpClient> &client,
												 const std::shared_ptr<mon::HostMetrics> &vhost)
		{
			return conv::JsonFromVHost(vhost);
		}
	}  // namespace v1
}  // namespace api
