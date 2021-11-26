//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "output_profiles_controller.h"

#include <orchestrator/orchestrator.h>

#include <functional>

#include "../../../../../api_private.h"

namespace api
{
	namespace v1
	{
		void OutputProfilesController::PrepareHandlers()
		{
			RegisterPost(R"()", &OutputProfilesController::OnPostOutputProfile);
			RegisterGet(R"()", &OutputProfilesController::OnGetOutputProfileList);
			RegisterGet(R"(\/(?<output_profile_name>[^\/]*))", &OutputProfilesController::OnGetOutputProfile);
			RegisterPut(R"(\/(?<output_profile_name>[^\/]*))", &OutputProfilesController::OnPutOutputProfile);
			RegisterDelete(R"(\/(?<output_profile_name>[^\/]*))", &OutputProfilesController::OnDeleteOutputProfile);
		};

		ov::String GetOutputProfileName(const std::shared_ptr<http::svr::HttpConnection> &client)
		{
			auto &match_result = client->GetRequest()->GetMatchResult();

			return match_result.GetNamedGroup("output_profile_name").GetValue();
		}

		off_t FindOutputProfile(const std::shared_ptr<mon::ApplicationMetrics> &app,
								const ov::String &output_profile_name,
								Json::Value *value)
		{
			auto &app_config = app->GetConfig();
			off_t offset = 0;

			for (auto &profile : app_config.GetOutputProfileList())
			{
				if (output_profile_name == profile.GetName().CStr())
				{
					if (value != nullptr)
					{
						*value = profile.ToJson();
					}

					return offset;
				}

				offset++;
			}

			return -1;
		}

		off_t FindOutputProfile(Json::Value &app_json,
								const ov::String &output_profile_name,
								Json::Value **value)
		{
			auto &output_profiles = app_json["outputProfiles"]["outputProfile"];
			off_t offset = 0;

			if (output_profiles.isArray())
			{
				for (auto &profile : output_profiles)
				{
					auto name = profile["name"];

					if (name.isString())
					{
						if (output_profile_name == name.asCString())
						{
							if (value != nullptr)
							{
								*value = &profile;
							}

							return offset;
						}
					}
					else
					{
						// Invalid name
						OV_ASSERT(false, "String is expected, but %d found", name.type());
					}

					offset++;
				}
			}

			return -1;
		}

		std::shared_ptr<const http::HttpError> CreateNotFoundError(const std::shared_ptr<mon::HostMetrics> &vhost,
																   const std::shared_ptr<mon::ApplicationMetrics> &app,
																   const ov::String &output_profile_name)
		{
			return http::HttpError::CreateError(
				http::StatusCode::NotFound,
				"Could not find the output profile: [%s/%s/%s]",
				vhost->GetName().CStr(), app->GetName().GetAppName().CStr(), output_profile_name.CStr());
		}

		std::shared_ptr<const http::HttpError> ChangeApp(const std::shared_ptr<mon::HostMetrics> &vhost,
														 const std::shared_ptr<mon::ApplicationMetrics> &app,
														 Json::Value &app_json)
		{
			// TODO(dimiden): Caution - Race condition may occur
			// If an application is deleted immediately after the GetApplication(),
			// the app information can no longer be obtained from Orchestrator
			auto orchestrator = ocst::Orchestrator::GetInstance();

			// Delete GET-only fields
			app_json.removeMember("dynamic");

			cfg::vhost::app::Application app_config;
			auto error = ::serdes::ApplicationFromJson(app_json, &app_config);

			if (error == nullptr)
			{
				if (ocst::Orchestrator::GetInstance()->DeleteApplication(*app) == ocst::Result::Failed)
				{
					return http::HttpError::CreateError(http::StatusCode::Forbidden, "Could not delete the application: [%s/%s]",
														vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
				}

				auto result = orchestrator->CreateApplication(*vhost, app_config);

				switch (result)
				{
					case ocst::Result::Failed:
						error = http::HttpError::CreateError(http::StatusCode::BadRequest, "Failed to create the application");
						break;

					case ocst::Result::Succeeded:
						break;

					case ocst::Result::Exists:
						error = http::HttpError::CreateError(http::StatusCode::Conflict, "The application already exists");
						break;

					case ocst::Result::NotExists:
						// CreateApplication() never returns NotExists
						error = http::HttpError::CreateError(http::StatusCode::InternalServerError, "Unknown error occurred");
						OV_ASSERT2(false);
						break;
				}
			}

			return http::HttpError::CreateError(error);
		}

		ApiResponse OutputProfilesController::OnPostOutputProfile(const std::shared_ptr<http::svr::HttpConnection> &client, const Json::Value &request_body,
																  const std::shared_ptr<mon::HostMetrics> &vhost,
																  const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			if (request_body.isArray() == false)
			{
				return http::HttpError::CreateError(http::StatusCode::BadRequest, "Request body must be an array");
			}

			MultipleStatus status_code;

			Json::Value app_json = app->GetConfig().ToJson();
			auto &output_profiles = app_json["outputProfiles"]["outputProfile"];

			Json::Value response(Json::ValueType::arrayValue);

			for (auto &item : request_body)
			{
				auto name = item["name"];

				if (name.isString() == false)
				{
					response.append(::serdes::JsonFromError(http::HttpError::CreateError(http::StatusCode::BadRequest, "Invalid name")));
					status_code.AddStatusCode(http::StatusCode::BadRequest);
				}
				else
				{
					if (FindOutputProfile(app_json, name.asCString(), nullptr) < 0)
					{
						output_profiles.append(item);

						response.append(name);
						status_code.AddStatusCode(http::StatusCode::OK);
					}
					else
					{
						response.append(::serdes::JsonFromError(http::HttpError::CreateError(http::StatusCode::Conflict, "The output profile \"%s\" already exists", name.asCString())));
						status_code.AddStatusCode(http::StatusCode::Conflict);
					}
				}
			}

			auto error = ChangeApp(vhost, app, app_json);
			Json::Value error_json = ::serdes::JsonFromError(error);

			std::shared_ptr<mon::ApplicationMetrics> new_app;
			Json::Value new_app_json;

			if (error == nullptr)
			{
				new_app = GetApplication(vhost, app->GetName().GetAppName().CStr());
				new_app_json = new_app->GetConfig().ToJson();
			}

			for (auto &response_profile : response)
			{
				if (response_profile.isString() == false)
				{
					// An error created above step
				}
				else
				{
					if (error == nullptr)
					{
						auto name = response_profile.asCString();
						Json::Value *new_profile;

						if (FindOutputProfile(new_app_json, name, &new_profile) < 0)
						{
							response_profile = ::serdes::JsonFromError(http::HttpError::CreateError(http::StatusCode::InternalServerError, "Output profile is created, but not found"));
							status_code.AddStatusCode(http::StatusCode::InternalServerError);
						}
						else
						{
							response_profile = Json::objectValue;
							response_profile["statusCode"] = static_cast<int>(http::StatusCode::OK);
							response_profile["message"] = StringFromStatusCode(http::StatusCode::OK);
							response_profile["response"] = *new_profile;
						}
					}
					else
					{
						response_profile = error_json;
						status_code.AddStatusCode(error);
					}
				}
			}

			if (status_code.HasOK())
			{
				cfg::ConfigManager::GetInstance()->SaveCurrentConfig();
			}

			return {status_code, std::move(response)};
		}

		ApiResponse OutputProfilesController::OnGetOutputProfileList(const std::shared_ptr<http::svr::HttpConnection> &client,
																	 const std::shared_ptr<mon::HostMetrics> &vhost,
																	 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response = Json::arrayValue;

			for (auto &item : app->GetConfig().GetOutputProfileList())
			{
				response.append(item.GetName().CStr());
			}

			return response;
		}

		ApiResponse OutputProfilesController::OnGetOutputProfile(const std::shared_ptr<http::svr::HttpConnection> &client,
																 const std::shared_ptr<mon::HostMetrics> &vhost,
																 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto profile_name = GetOutputProfileName(client);

			for (auto &profile : app->GetConfig().GetOutputProfileList())
			{
				if (profile_name == profile.GetName().CStr())
				{
					return profile.ToJson();
				}
			}

			return CreateNotFoundError(vhost, app, profile_name);
		}

		ApiResponse OutputProfilesController::OnPutOutputProfile(const std::shared_ptr<http::svr::HttpConnection> &client, const Json::Value &request_body,
																 const std::shared_ptr<mon::HostMetrics> &vhost,
																 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			if (request_body.isObject() == false)
			{
				return http::HttpError::CreateError(http::StatusCode::BadRequest, "Request body must be an object");
			}

			auto profile_name = GetOutputProfileName(client);
			Json::Value app_json = app->GetConfig().ToJson();
			Json::Value *output_profile_json;

			off_t index = FindOutputProfile(app_json, profile_name, &output_profile_json);

			if (index < 0)
			{
				return CreateNotFoundError(vhost, app, profile_name);
			}

			for (auto item = request_body.begin(); item != request_body.end(); ++item)
			{
				ov::String name = item.name().c_str();
				auto lower_name = name.LowerCaseString();

				// Prevent to change the name using this API
				if (lower_name == "name")
				{
					return http::HttpError::CreateError(http::StatusCode::BadRequest, "The %s entry cannot be specified in the modification", name.CStr());
				}
			}

			Json::Value request_json = request_body;

			request_json["name"] = std::string(profile_name);

			// Modify the json object
			*output_profile_json = request_json;

			auto error = ChangeApp(vhost, app, app_json);

			if (error == nullptr)
			{
				auto new_app = GetApplication(vhost, app->GetName().GetAppName().CStr());

				Json::Value value;

				if (FindOutputProfile(new_app, profile_name, &value) < 0)
				{
					return http::HttpError::CreateError(http::StatusCode::InternalServerError, "Output profile is modified, but not found");
				}

				cfg::ConfigManager::GetInstance()->SaveCurrentConfig();

				return value;
			}

			return error;
		}

		ApiResponse OutputProfilesController::OnDeleteOutputProfile(const std::shared_ptr<http::svr::HttpConnection> &client,
																	const std::shared_ptr<mon::HostMetrics> &vhost,
																	const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto profile_name = GetOutputProfileName(client);
			off_t index = FindOutputProfile(app, profile_name, nullptr);

			if (index < 0)
			{
				return CreateNotFoundError(vhost, app, profile_name);
			}

			Json::Value app_json = app->GetConfig().ToJson();
			auto &output_profiles = app_json["outputProfiles"]["outputProfile"];

			if (output_profiles.removeIndex(index, nullptr) == false)
			{
				return http::HttpError::CreateError(http::StatusCode::Forbidden, "Could not delete output profile");
			}

			auto error = ChangeApp(vhost, app, app_json);

			if (error == nullptr)
			{
				cfg::ConfigManager::GetInstance()->SaveCurrentConfig();
			}

			return error;
		}
	}  // namespace v1
}  // namespace api
