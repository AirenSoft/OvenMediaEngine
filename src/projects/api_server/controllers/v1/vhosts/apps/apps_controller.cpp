//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "apps_controller.h"

#include <orchestrator/orchestrator.h>

#include <functional>

#include "../../../../api_private.h"
#include "../../../../converters/converters.h"
#include "../../../../helpers/helpers.h"

namespace api
{
	namespace v1
	{
		void AppsController::PrepareHandlers()
		{
			RegisterGet(R"()", &AppsController::OnGetAppList);
			RegisterGet(R"(\/(?<app_name>[^\/]*))", &AppsController::OnGetApp);
		};

		ApiResponse AppsController::OnGetAppList(const std::shared_ptr<HttpClient> &client)
		{
			auto &match_result = client->GetRequest()->GetMatchResult();
			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);

			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]", vhost_name.length(), vhost_name.data());
			}

			Json::Value response;

			for (auto &item : vhost->app_map)
			{
				auto &app = item.second;

				response.append(app->app_info.GetName().GetAppName().CStr());
			}

			return response;
		}

		ApiResponse AppsController::OnGetApp(const std::shared_ptr<HttpClient> &client)
		{
			auto &match_result = client->GetRequest()->GetMatchResult();
			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto app_name = match_result.GetNamedGroup("app_name");

			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]", vhost_name.length(), vhost_name.data());
			}

			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find app: [%.*s]", app_name.length(), app_name.data());
			}

			return api::conv::ConvertFromApplication(app);
		}
	}  // namespace v1
}  // namespace api
