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
#include "../../../api_server.h"
#include "apps/apps_controller.h"

namespace api
{
	namespace v1
	{
		void VHostsController::PrepareHandlers()
		{
			RegisterGet(R"()", &VHostsController::OnGetVhostList);
			RegisterGet(R"(\/(?<vhost_name>[^\/]*))", &VHostsController::OnGetVhost);

			RegisterDelete(R"(\/(?<vhost_name>[^\/]*))", &VHostsController::OnDeleteVhost);

			CreateSubController<AppsController>(R"(\/(?<vhost_name>[^\/]*)\/apps)");
		}

		ApiResponse VHostsController::OnGetVhostList(const std::shared_ptr<http::svr::HttpConnection> &client)
		{
			auto vhost_list = GetVirtualHostList();
			Json::Value response(Json::ValueType::arrayValue);

			for (const auto &item : vhost_list)
			{
				response.append(item.second->GetName().CStr());
			}

			return response;
		}

		ApiResponse VHostsController::OnGetVhost(const std::shared_ptr<http::svr::HttpConnection> &client,
												 const std::shared_ptr<mon::HostMetrics> &vhost)
		{
			return ::serdes::JsonFromVHost(vhost);
		}

		ApiResponse VHostsController::OnDeleteVhost(const std::shared_ptr<http::svr::HttpConnection> &client,
													const std::shared_ptr<mon::HostMetrics> &vhost)
		{
			auto api_server = Server::GetInstance();

			try
			{
				api_server->DeleteVHost(*(vhost.get()));
			}
			catch (std::shared_ptr<cfg::ConfigError> &error)
			{
				logte("An error occurred while load API config: %s", error->ToString().CStr());
				return http::HttpError::CreateError(error);
			}

			return http::StatusCode::OK;
		}
	}  // namespace v1
}  // namespace api
