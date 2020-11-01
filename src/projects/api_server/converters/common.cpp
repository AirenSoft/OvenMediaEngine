//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./common.h"

namespace api
{
	namespace conv
	{
		void SetTimestamp(Json::Value &parent_object, const char *key, const std::chrono::system_clock::time_point &time_point)
		{
			// object = ov::Converter::ToISO860
		}
	}  // namespace conv
}  // namespace api