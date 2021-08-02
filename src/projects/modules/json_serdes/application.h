//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <monitoring/monitoring.h>
#include <modules/http/server/http_server.h>

namespace serdes
{
	Json::Value JsonFromOutputProfile(const cfg::vhost::app::oprf::OutputProfile &output_profile);
	Json::Value JsonFromApplication(const std::shared_ptr<const mon::ApplicationMetrics> &application);
	std::shared_ptr<http::HttpError> ApplicationFromJson(const Json::Value &json_value, cfg::vhost::app::Application *application);
}  // namespace serdes