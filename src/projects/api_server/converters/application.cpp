//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "application.h"

#include <config/config.h>

#include "common.h"

namespace api
{
	namespace conv
	{
		Json::Value JsonFromOutputProfile(const cfg::vhost::app::oprf::OutputProfile &output_profile)
		{
			return output_profile.ToJson();
		}

		Json::Value JsonFromApplication(const std::shared_ptr<const mon::ApplicationMetrics> &application)
		{
			Json::Value app = cfg::conv::GetApplicationFromMetrics(application);

			if (app.isObject())
			{
				app["dynamic"] = application->IsDynamicApp();
			}

			return app;
		}

		std::shared_ptr<http::HttpError> ApplicationFromJson(const Json::Value &json_value, cfg::vhost::app::Application *application)
		{
			cfg::DataSource data_source("", "", "Application", json_value);

			try
			{
				application->FromDataSource("application", "Application", data_source);
				return nullptr;
			}
			catch (const std::shared_ptr<cfg::ConfigError> &error)
			{
				return http::HttpError::CreateError(http::StatusCode::BadRequest, "%s", error->GetMessage().CStr());
			}
		}
	}  // namespace conv
}  // namespace api
