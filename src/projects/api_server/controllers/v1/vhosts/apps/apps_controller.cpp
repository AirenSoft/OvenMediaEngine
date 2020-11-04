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
#include "streams/streams_controller.h"

namespace api
{
	namespace v1
	{
		void AppsController::PrepareHandlers()
		{
			RegisterPost(R"()", &AppsController::OnPostApp);
			RegisterGet(R"()", &AppsController::OnGetAppList);
			RegisterGet(R"(\/(?<app_name>[^\/]*))", &AppsController::OnGetApp);
			RegisterPut(R"(\/(?<app_name>[^\/]*))", &AppsController::OnPutApp);
			RegisterDelete(R"(\/(?<app_name>[^\/]*))", &AppsController::OnDeleteApp);

			CreateSubController<v1::StreamsController>(R"(\/(?<app_name>[^\/]*)\/streams)");
		};

		ApiResponse AppsController::OnPostApp(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body)
		{
			logtd(">> %s", ov::Json::Stringify(request_body, true).CStr());

			// auto orchestrator = ocst::Orchestrator::GetInstance();

			cfg::Application app;

			// orchestrator->CreateApplication(
			
			return HttpStatusCode::NotImplemented;
		}

		ApiResponse AppsController::OnGetAppList(const std::shared_ptr<HttpClient> &client)
		{
			auto &match_result = client->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			Json::Value response = Json::arrayValue;

			auto app_list = GetApplicationList(vhost);

			for (auto &item : app_list)
			{
				auto &app = item.second;

				response.append(app->GetName().GetAppName().CStr());
			}

			return response;
		}

		ApiResponse AppsController::OnGetApp(const std::shared_ptr<HttpClient> &client)
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

			return api::conv::JsonFromApplication(app);
		}

		ApiResponse AppsController::OnPutApp(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body)
		{
			logtd(">> %s", ov::Json::Stringify(request_body, true).CStr());

			return HttpStatusCode::NotImplemented;
		}

		ApiResponse AppsController::OnDeleteApp(const std::shared_ptr<HttpClient> &client)
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

			if (ocst::Orchestrator::GetInstance()->DeleteApplication(*app) == ocst::Result::Failed)
			{
				return ov::Error::CreateError(HttpStatusCode::Forbidden, "Could not delete application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}

			return {};
		}
	}  // namespace v1
}  // namespace api
