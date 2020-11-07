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
#include "base/info/record.h"

namespace api
{
	namespace conv
	{
		std::shared_ptr<info::Record>  RecordFromJson(const Json::Value &json_body);
	
	}  // namespace conv
};	   // namespace api