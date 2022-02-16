//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "apps_controller.h"

#include <config/config.h>
#include <modules/json_serdes/converters.h>
#include <orchestrator/orchestrator.h>

#include <functional>

#include "../../../../api_private.h"
#include "app_actions_controller.h"
#include "output_profiles/output_profiles_controller.h"
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
			CreateSubController<AppActionsController>(R"(\/(?<app_name>[^\/:]*):)");

			// Branch into stream controller
			CreateSubController<StreamsController>(R"(\/(?<app_name>[^\/:]*)\/streams)");

			// Branch into output profile controller
			CreateSubController<OutputProfilesController>(R"(\/(?<app_name>[^\/:]*)\/outputProfiles)");
		};

		bool FillDefaultValues(Json::Value &config)
		{
			// Setting up the default values
			if (config.isMember("providers") == false)
			{
				auto &providers = config["providers"];

				providers["rtmp"] = Json::objectValue;
				providers["mpegts"] = Json::objectValue;
			}

			if (config.isMember("publishers") == false)
			{
				auto &publishers = config["publishers"];

				publishers["hls"] = Json::objectValue;
				publishers["dash"] = Json::objectValue;
				publishers["llDash"] = Json::objectValue;
				publishers["webrtc"] = Json::objectValue;
			}

			if (config.isMember("outputProfiles") == false)
			{
				Json::Value output_profile(Json::objectValue);

				output_profile["name"] = "bypass";
				output_profile["outputStreamName"] = "${OriginStreamName}";

				Json::Value codec;

				codec["bypass"] = true;

				auto &encodes = output_profile["encodes"];
				encodes["videos"].append(codec);
				encodes["audios"].append(codec);

				codec = Json::objectValue;
				codec["codec"] = "opus";
				codec["bitrate"] = 128000;
				codec["samplerate"] = 48000;
				codec["channel"] = 2;
				encodes["audios"].append(codec);

				config["outputProfiles"]["outputProfile"].append(output_profile);
			}

			return true;
		}

		ApiResponse AppsController::OnPostApp(const std::shared_ptr<http::svr::HttpConnection> &client, const Json::Value &request_body,
											  const std::shared_ptr<mon::HostMetrics> &vhost)
		{
			ThrowIfVirtualIsReadOnly();

			if (request_body.isArray() == false)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Request body must be an array");
			}

			auto orchestrator = ocst::Orchestrator::GetInstance();
			Json::Value response_value(Json::ValueType::arrayValue);
			// Copy values to fill default values
			Json::Value requested_app_list = request_body;

			MultipleStatus status_codes;

			for (auto &requested_app : requested_app_list)
			{
				FillDefaultValues(requested_app);

				cfg::vhost::app::Application app_config;
				::serdes::ApplicationFromJson(requested_app, &app_config);

				try
				{
					app_config.FromJson(requested_app);

					auto result = orchestrator->CreateApplication(*vhost, app_config);

					switch (result)
					{
						case ocst::Result::Failed:
							throw http::HttpError(http::StatusCode::BadRequest,
												  "Failed to create the application: [%s/%s]",
												  vhost->GetName().CStr(), app_config.GetName().CStr());

						case ocst::Result::Succeeded:
							break;

						case ocst::Result::Exists:
							throw http::HttpError(http::StatusCode::Conflict,
												  "The application already exists: [%s/%s]",
												  vhost->GetName().CStr(), app_config.GetName().CStr());

						case ocst::Result::NotExists:
							// CreateApplication() never returns NotExists
							OV_ASSERT2(false);
							throw http::HttpError(http::StatusCode::InternalServerError,
												  "Unknown error occurred: [%s/%s]",
												  vhost->GetName().CStr(), app_config.GetName().CStr());
					}

					auto app = GetApplication(vhost, app_config.GetName().CStr());
					auto app_json = ::serdes::JsonFromApplication(app);

					Json::Value response;
					response["statusCode"] = static_cast<int>(http::StatusCode::OK);
					response["message"] = StringFromStatusCode(http::StatusCode::OK);
					response["response"] = app_json;

					status_codes.AddStatusCode(http::StatusCode::OK);
					response_value.append(std::move(response));
				}
				catch (const cfg::ConfigError &error)
				{
					auto http_error = http::HttpError(http::StatusCode::BadRequest, error.What());

					status_codes.AddStatusCode(http_error.GetStatusCode());
					response_value.append(::serdes::JsonFromError(http_error));
				}
				catch (const http::HttpError &error)
				{
					status_codes.AddStatusCode(error.GetStatusCode());
					response_value.append(::serdes::JsonFromError(error));
				}
			}

			return {status_codes, std::move(response_value)};
		}

		ApiResponse AppsController::OnGetAppList(const std::shared_ptr<http::svr::HttpConnection> &client,
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

		ApiResponse AppsController::OnGetApp(const std::shared_ptr<http::svr::HttpConnection> &client,
											 const std::shared_ptr<mon::HostMetrics> &vhost,
											 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			return ::serdes::JsonFromApplication(app);
		}

		void OverwriteJson(const Json::Value &from, Json::Value &to)
		{
			for (auto item = from.begin(); item != from.end(); ++item)
			{
				switch (item->type())
				{
					case Json::ValueType::nullValue:
						[[fallthrough]];
					case Json::ValueType::intValue:
						[[fallthrough]];
					case Json::ValueType::uintValue:
						[[fallthrough]];
					case Json::ValueType::realValue:
						[[fallthrough]];
					case Json::ValueType::stringValue:
						[[fallthrough]];
					case Json::ValueType::booleanValue:
						[[fallthrough]];
					case Json::ValueType::arrayValue:
						to[item.name()] = *item;
						break;

					case Json::ValueType::objectValue: {
						OverwriteJson(*item, to[item.name()]);
					}
				}
			}
		}

		ApiResponse AppsController::OnPutApp(const std::shared_ptr<http::svr::HttpConnection> &client, const Json::Value &request_body,
											 const std::shared_ptr<mon::HostMetrics> &vhost,
											 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			ThrowIfVirtualIsReadOnly();

			if (request_body.isObject() == false)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Request body must be an object");
			}

			// TODO(dimiden): Caution - Race condition may occur
			// If an application is deleted immediately after the GetApplication(),
			// the app information can no longer be obtained from Orchestrator

			auto orchestrator = ocst::Orchestrator::GetInstance();

			auto app_json = ::serdes::JsonFromApplication(app);

			// Delete GET-only fields
			app_json.removeMember("dynamic");

			// Prevent to change the name/outputProfiles using this API
			if (request_body.isMember("name"))
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Cannot change [name] using this API");
			}

			if (request_body.isMember("dynamic"))
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Cannot change [dynamic] using this API");
			}

			if (request_body.isMember("outputProfiles"))
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Cannot change [outputProfiles] using this API");
			}

			// Copy request_body into app_json
			OverwriteJson(request_body, app_json);

			cfg::vhost::app::Application app_config;
			::serdes::ApplicationFromJson(app_json, &app_config);

			if (ocst::Orchestrator::GetInstance()->DeleteApplication(*app) == ocst::Result::Failed)
			{
				throw http::HttpError(http::StatusCode::Forbidden,
									  "Could not delete the application: [%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			auto result = orchestrator->CreateApplication(*vhost, app_config);

			switch (result)
			{
				case ocst::Result::Failed:
					throw http::HttpError(http::StatusCode::BadRequest,
										  "Failed to create the application: [%s/%s]",
										  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());

				case ocst::Result::Succeeded:
					break;

				case ocst::Result::Exists:
					throw http::HttpError(http::StatusCode::Conflict,
										  "The application already exists: [%s/%s]",
										  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());

				case ocst::Result::NotExists:
					// CreateApplication() never returns NotExists
					OV_ASSERT2(false);
					throw http::HttpError(http::StatusCode::InternalServerError,
										  "Unknown error occurred: [%s/%s]",
										  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			auto app_metrics = GetApplication(vhost, app_config.GetName().CStr());

			return ::serdes::JsonFromApplication(app_metrics);
		}

		ApiResponse AppsController::OnDeleteApp(const std::shared_ptr<http::svr::HttpConnection> &client,
												const std::shared_ptr<mon::HostMetrics> &vhost,
												const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			ThrowIfVirtualIsReadOnly();
			
			switch (ocst::Orchestrator::GetInstance()->DeleteApplication(*app))
			{
				case ocst::Result::Failed:
					throw http::HttpError(http::StatusCode::BadRequest,
										  "Failed to delete the application: [%s/%s]",
										  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());

				case ocst::Result::Succeeded:
					break;

				case ocst::Result::Exists:
					// CreateVirtDeleteVirtualHostualHost() never returns Exists
					OV_ASSERT2(false);
					throw http::HttpError(http::StatusCode::InternalServerError,
										  "Unknown error occurred: [%s/%s]",
										  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());

				case ocst::Result::NotExists:
					// CreateVirtualHost() never returns NotExists
					throw http::HttpError(http::StatusCode::NotFound,
										  "The virtual host not exists: [%s/%s]",
										  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());

					throw http::HttpError(http::StatusCode::Forbidden,
										  "Could not delete the application: [%s/%s]",
										  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			return {};
		}
	}  // namespace v1
}  // namespace api
