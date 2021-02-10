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
			return std::move(output_profile.ToJson());
		}

		Json::Value JsonFromApplication(const std::shared_ptr<const mon::ApplicationMetrics> &application)
		{
			Json::Value app = cfg::conv::GetApplicationFromMetrics(application);

			if (app.isObject())
			{
				app["dynamic"] = application->IsDynamicApp();
			}

			return std::move(app);
		}

		std::shared_ptr<HttpError> ApplicationFromJson(const Json::Value &json_value, cfg::vhost::app::Application *application)
		{
			cfg::DataSource data_source("", "", "Application", json_value);

			try
			{
				application->FromDataSource("application", "Application", data_source);
				return nullptr;
			}
			catch (const std::shared_ptr<cfg::ConfigError> &error)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, "%s", error->GetMessage().CStr());
			}
		}
	}  // namespace conv
}  // namespace api
