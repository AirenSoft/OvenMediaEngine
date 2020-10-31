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
		Json::Value ConvertFromStream(const std::shared_ptr<const mon::StreamMetrics> &stream, std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams);
	}  // namespace conv
};	   // namespace api