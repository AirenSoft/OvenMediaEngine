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
#include "app_actions_controller.h"
#include "streams/streams_controller.h"

namespace api
{
	namespace v1
	{
		void AppsController::PrepareHandlers()
		{
			RegisterPost(R"()", &AppsController::OnPostApp);
			RegisterGet(R"()", &AppsController::OnGetAppList);
			RegisterGet(R"(\/(?<app_name>[^\/:]*))", &AppsController::OnGetApp);
			RegisterPut(R"(\/(?<app_name>[^\/:]*))", &AppsController::OnPutApp);
			RegisterDelete(R"(\/(?<app_name>[^\/:]*))", &AppsController::OnDeleteApp);

			// Branch into action controller
			CreateSubController<v1::AppActionsController>(R"(\/(?<app_name>[^\/:]*):)");

			// Branch into stream controller
			CreateSubController<v1::StreamsController>(R"(\/(?<app_name>[^\/:]*)\/streams)");
		};

		ApiResponse AppsController::OnPostApp(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
											  const std::shared_ptr<mon::HostMetrics> &vhost)
		{
			if (request_body.isArray() == false)
			{
				return ov::Error::CreateError(HttpStatusCode::BadRequest, "Request body must be an array");
			}

			auto orchestrator = ocst::Orchestrator::GetInstance();
			Json::Value response_value(Json::ValueType::arrayValue);
			Json::Value requested_config = request_body;

			for (auto &item : requested_config)
			{
				cfg::vhost::app::Application app_config;

				// Setting up the default values
				if (item["providers"].isNull())
				{
					item["providers"]["rtmp"] = Json::objectValue;
					item["providers"]["mpegts"] = Json::objectValue;
				}

				if (item["publishers"].isNull())
				{
					item["publishers"]["hls"] = Json::objectValue;
					item["publishers"]["dash"] = Json::objectValue;
					item["publishers"]["lldash"] = Json::objectValue;
					item["publishers"]["webrtc"] = Json::objectValue;
				}

				auto error = conv::ApplicationFromJson(item, &app_config);

				if (error == nullptr)
				{
					auto result = orchestrator->CreateApplication(*vhost, app_config);

					switch (result)
					{
						case ocst::Result::Failed:
							error = ov::Error::CreateError(HttpStatusCode::BadRequest, "Failed to create the application");
							break;

						case ocst::Result::Succeeded:
							break;

						case ocst::Result::Exists:
							error = ov::Error::CreateError(HttpStatusCode::BadRequest, "The application already exists");
							break;

						case ocst::Result::NotExists:
							// CreateApplication() never returns NotExists
							error = ov::Error::CreateError(HttpStatusCode::InternalServerError, "Unknown error occurred");
							OV_ASSERT2(false);
							break;
					}
				}

				if (error != nullptr)
				{
					Json::Value error_value;
					error_value["message"] = error->ToString().CStr();
					response_value.append(error_value);
				}
				else
				{
					auto app = GetApplication(vhost, app_config.GetName().CStr());
					response_value.append(conv::JsonFromApplication(app));
				}
			}

			return response_value;
		}

		ApiResponse AppsController::OnGetAppList(const std::shared_ptr<HttpClient> &client,
												 const std::shared_ptr<mon::HostMetrics> &vhost)
		{
			Json::Value response(Json::ValueType::arrayValue);

			auto app_list = GetApplicationList(vhost);

			for (auto &item : app_list)
			{
				auto &app = item.second;

				response.append(app->GetName().GetAppName().CStr());
			}

			return response;
		}

		ApiResponse AppsController::OnGetApp(const std::shared_ptr<HttpClient> &client,
											 const std::shared_ptr<mon::HostMetrics> &vhost,
											 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			return api::conv::JsonFromApplication(app);
		}

		ApiResponse AppsController::OnPutApp(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
											 const std::shared_ptr<mon::HostMetrics> &vhost,
											 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			// TODO(dimiden): Caution - Race condition may occur
			// If an application is deleted immediately after the GetApplication(),
			// the app information can no longer be obtained from Orchestrator

			auto orchestrator = ocst::Orchestrator::GetInstance();
			Json::Value requested_config = request_body;

			for (auto &item : requested_config)
			{
				cfg::vhost::app::Application app_config = app->GetConfig();
				auto error = conv::ApplicationFromJson(item, &app_config);

				if (error == nullptr)
				{
					if (ocst::Orchestrator::GetInstance()->DeleteApplication(*app) == ocst::Result::Failed)
					{
						return ov::Error::CreateError(HttpStatusCode::Forbidden, "Could not delete the application: [%s/%s]",
													  vhost->GetName().CStr(), app->GetName().CStr());
					}

					auto result = orchestrator->CreateApplication(*vhost, app_config);

					switch (result)
					{
						case ocst::Result::Failed:
							error = ov::Error::CreateError(HttpStatusCode::BadRequest, "Failed to create the application");
							break;

						case ocst::Result::Succeeded:
							break;

						case ocst::Result::Exists:
							error = ov::Error::CreateError(HttpStatusCode::BadRequest, "The application already exists");
							break;

						case ocst::Result::NotExists:
							// CreateApplication() never returns NotExists
							error = ov::Error::CreateError(HttpStatusCode::InternalServerError, "Unknown error occurred");
							OV_ASSERT2(false);
							break;
					}
				}

				if (error != nullptr)
				{
					Json::Value error_value;
					error_value["message"] = error->ToString().CStr();
					// response_value.append(error_value);
				}
				else
				{
					auto app = GetApplication(vhost, app_config.GetName().CStr());
					// response_value.append(conv::JsonFromApplication(app));
				}
			}

			// return response_value;
			return {};
		}

		ApiResponse AppsController::OnDeleteApp(const std::shared_ptr<HttpClient> &client,
												const std::shared_ptr<mon::HostMetrics> &vhost,
												const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			if (ocst::Orchestrator::GetInstance()->DeleteApplication(*app) == ocst::Result::Failed)
			{
				return ov::Error::CreateError(HttpStatusCode::Forbidden, "Could not delete the application: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().CStr());
			}

			return Json::Value(Json::ValueType::objectValue);
		}
	}  // namespace v1
}  // namespace api
