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
#include "../../../converters/converters.h"
#include "../../../helpers/helpers.h"
#include "apps/apps_controller.h"

namespace api
{
	namespace v1
	{
		void VHostsController::PrepareHandlers()
		{
			RegisterGet(R"()", &VHostsController::OnGetVhostList);
			RegisterGet(R"(\/(?<vhost_name>[^\/]*))", &VHostsController::OnGetVhost);

			CreateSubController<v1::AppsController>(R"(\/(?<vhost_name>[^\/]*)\/apps)");
		}

		ApiResponse VHostsController::OnGetVhostList(const std::shared_ptr<HttpClient> &client)
		{
			const auto &vhost_list = ocst::Orchestrator::GetInstance()->GetVirtualHostList();
			Json::Value response;

			for (const auto &vhost : vhost_list)
			{
				response.append(vhost->name.CStr());
			}

			return response;
		}

		ApiResponse VHostsController::OnGetVhost(const std::shared_ptr<HttpClient> &client)
		{
			// Get resources from URI
			auto &match_result = client->GetRequest()->GetMatchResult();
			auto vhost_name = match_result.GetNamedGroup("vhost_name");

			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]", vhost_name.length(), vhost_name.data());
			}

			return api::conv::ConvertFromVHost(vhost);
		}

	}  // namespace v1
}  // namespace api
