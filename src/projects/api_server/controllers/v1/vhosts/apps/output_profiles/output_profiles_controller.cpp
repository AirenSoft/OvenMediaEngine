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

		std::string_view GetOutputProfileName(const std::shared_ptr<HttpClient> &client)
		{
			auto &match_result = client->GetRequest()->GetMatchResult();

			return match_result.GetNamedGroup("output_profile_name");
		}

		off_t FindOutputProfile(const std::shared_ptr<mon::ApplicationMetrics> &app,
								const std::string_view &output_profile_name,
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
								const std::string_view &output_profile_name,
								Json::Value **value)
		{
			auto &output_profiles = app_json["outputProfiles"];
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

		std::shared_ptr<HttpError> CreateNotFoundError(const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app,
													   const std::string_view &output_profile_name)
		{
			return HttpError::CreateError(
				HttpStatusCode::NotFound,
				"Could not find the output profile: [%s/%s/%.*s]",
				vhost->GetName().CStr(), app->GetName().GetAppName().CStr(),
				output_profile_name.length(), output_profile_name.data());
		}

		std::shared_ptr<HttpError> ChangeApp(const std::shared_ptr<mon::HostMetrics> &vhost,
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
			auto error = conv::ApplicationFromJson(app_json, &app_config);

			if (error == nullptr)
			{
				if (ocst::Orchestrator::GetInstance()->DeleteApplication(*app) == ocst::Result::Failed)
				{
					return HttpError::CreateError(HttpStatusCode::Forbidden, "Could not delete the application: [%s/%s]",
												  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
				}

				auto result = orchestrator->CreateApplication(*vhost, app_config);

				switch (result)
				{
					case ocst::Result::Failed:
						error = HttpError::CreateError(HttpStatusCode::BadRequest, "Failed to create the application");
						break;

					case ocst::Result::Succeeded:
						break;

					case ocst::Result::Exists:
						error = HttpError::CreateError(HttpStatusCode::Conflict, "The application already exists");
						break;

					case ocst::Result::NotExists:
						// CreateApplication() never returns NotExists
						error = HttpError::CreateError(HttpStatusCode::InternalServerError, "Unknown error occurred");
						OV_ASSERT2(false);
						break;
				}
			}

			return HttpError::CreateError(error);
		}

		ApiResponse OutputProfilesController::OnPostOutputProfile(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
																  const std::shared_ptr<mon::HostMetrics> &vhost,
																  const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			if (request_body.isArray() == false)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, "Request body must be an array");
			}

			MultipleStatus status_code;

			Json::Value app_json = app->GetConfig().ToJson();
			auto &output_profiles = app_json["outputProfiles"];

			Json::Value response(Json::ValueType::arrayValue);

			for (auto &item : request_body)
			{
				auto name = item["name"];

				if (name.isString() == false)
				{
					response.append(conv::JsonFromError(HttpError::CreateError(HttpStatusCode::BadRequest, "Invalid name")));
					status_code.AddStatusCode(HttpStatusCode::BadRequest);
				}
				else
				{
					if (FindOutputProfile(app_json, name.asCString(), nullptr) < 0)
					{
						output_profiles.append(item);

						response.append(name);
						status_code.AddStatusCode(HttpStatusCode::OK);
					}
					else
					{
						response.append(conv::JsonFromError(HttpError::CreateError(HttpStatusCode::Conflict, "The output profile \"%s\" already exists", name.asCString())));
						status_code.AddStatusCode(HttpStatusCode::Conflict);
					}
				}
			}

			auto error = ChangeApp(vhost, app, app_json);
			Json::Value error_json = conv::JsonFromError(error);

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
						auto name = response_profile.asString();
						Json::Value *new_profile;

						if (FindOutputProfile(new_app_json, name, &new_profile) < 0)
						{
							response_profile = conv::JsonFromError(HttpError::CreateError(HttpStatusCode::InternalServerError, "Output profile is created, but not found"));
							status_code.AddStatusCode(HttpStatusCode::InternalServerError);
						}
						else
						{
							response_profile = Json::objectValue;
							response_profile["statusCode"] = static_cast<int>(HttpStatusCode::OK);
							response_profile["message"] = StringFromHttpStatusCode(HttpStatusCode::OK);
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

		ApiResponse OutputProfilesController::OnGetOutputProfileList(const std::shared_ptr<HttpClient> &client,
																	 const std::shared_ptr<mon::HostMetrics> &vhost,
																	 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response = Json::arrayValue;

			for (auto &item : app->GetConfig().GetOutputProfileList())
			{
				response.append(item.GetName().CStr());
			}

			return std::move(response);
		}

		ApiResponse OutputProfilesController::OnGetOutputProfile(const std::shared_ptr<HttpClient> &client,
																 const std::shared_ptr<mon::HostMetrics> &vhost,
																 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto profile_name = GetOutputProfileName(client);

			for (auto &profile : app->GetConfig().GetOutputProfileList())
			{
				if (profile_name == profile.GetName().CStr())
				{
					return std::move(profile.ToJson());
				}
			}

			return CreateNotFoundError(vhost, app, profile_name);
		}

		ApiResponse OutputProfilesController::OnPutOutputProfile(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
																 const std::shared_ptr<mon::HostMetrics> &vhost,
																 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			if (request_body.isObject() == false)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, "Request body must be an object");
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
					return HttpError::CreateError(HttpStatusCode::BadRequest, "The %s entry cannot be specified in the modification", name.CStr());
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
					return HttpError::CreateError(HttpStatusCode::InternalServerError, "Output profile is modified, but not found");
				}

				cfg::ConfigManager::GetInstance()->SaveCurrentConfig();

				return std::move(value);
			}

			return error;
		}

		ApiResponse OutputProfilesController::OnDeleteOutputProfile(const std::shared_ptr<HttpClient> &client,
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
			auto &output_profiles = app_json["outputProfiles"];

			if (output_profiles.removeIndex(index, nullptr) == false)
			{
				return HttpError::CreateError(HttpStatusCode::Forbidden, "Could not delete output profile");
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
