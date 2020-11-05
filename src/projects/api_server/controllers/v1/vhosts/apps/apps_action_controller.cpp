//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "apps_action_controller.h"

#include <orchestrator/orchestrator.h>

#include <functional>

#include "../../../../api_private.h"
#include "../../../../converters/converters.h"
#include "../../../../helpers/helpers.h"

namespace api
{
	namespace v1
	{
		void AppsActionController::PrepareHandlers()
		{
			RegisterGet(R"()", &AppsActionController::OnGetDummyAction);
			
			// Record related actions
			RegisterGet(R"((records))", &AppsActionController::OnGetDummyAction);
			RegisterPost(R"((startRecord))", &AppsActionController::OnPostDummyAction);
			RegisterPost(R"((stopRecord))", &AppsActionController::OnPostDummyAction);

			// Push related actions
			RegisterGet(R"((pushes))", &AppsActionController::OnGetDummyAction);
			RegisterPost(R"((startPush))", &AppsActionController::OnPostDummyAction);
			RegisterPost(R"((stopPush))", &AppsActionController::OnPostDummyAction);			
		};

		ApiResponse AppsActionController::OnGetDummyAction(const std::shared_ptr<HttpClient> &client)
		{
			auto &match_result = client->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			auto app_name = match_result.GetNamedGroup("app_name");
			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}

			logte("Called OnGetDummyAction. invoke [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());


			return api::conv::JsonFromApplication(app);
		}

		ApiResponse AppsActionController::OnPostDummyAction(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body)
		{
			auto &match_result = client->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			auto app_name = match_result.GetNamedGroup("app_name");
			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}

			logte("Called OnPostDummyAction. invoke [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());


			return api::conv::JsonFromApplication(app);
		}
		
	}  // namespace v1
}  // namespace api
