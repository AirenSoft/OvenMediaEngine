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

namespace api
{
	namespace conv
	{
		Json::Value ConvertFromApplication(const std::shared_ptr<const mon::ApplicationMetrics> &application);
	}  // namespace conv
};	   // namespace api