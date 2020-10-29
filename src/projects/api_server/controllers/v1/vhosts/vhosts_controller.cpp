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
			RegisterGet("", &VHostsController::OnGetVhostList);
			RegisterGet("/(?<vhost_name>[^/]*)", &VHostsController::OnGetVhost);

			CreateSubController<v1::AppsController>("/(?<vhost_name>[^/]*)/apps");
		}

		ApiResponse VHostsController::OnGetVhostList(const std::shared_ptr<HttpClient> &client)
		{
			auto vhost_list_config = cfg::ConfigManager::GetInstance()->GetServer()->GetVirtualHostList();
			Json::Value response(Json::ValueType::arrayValue);

			for (const auto &vhost : vhost_list_config)
			{
				response.append(vhost.GetName().CStr());
			}

			return response;
		}

		ApiResponse VHostsController::OnGetVhost(const std::shared_ptr<HttpClient> &client)
		{
			// Get resources from URI
			auto &match_result = client->GetRequest()->GetMatchResult();
			auto vhost_name = match_result.GetNamedGroup("vhost_name");

			auto vhost_list_config = cfg::ConfigManager::GetInstance()->GetServer()->GetVirtualHostList();

			for (const auto &vhost : vhost_list_config)
			{
				if (vhost_name == vhost.GetName().CStr())
				{
					// TODO(dimiden): Fill this information
					Json::Value response(Json::ValueType::objectValue);
					response["name"] = vhost.GetName().CStr();
					return response;
				}
			}

			return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]", vhost_name.length(), vhost_name.data());
		}

	}  // namespace v1
}  // namespace api
