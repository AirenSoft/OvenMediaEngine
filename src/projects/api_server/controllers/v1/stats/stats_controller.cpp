//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stats_controller.h"

#include "current/current_controller.h"

namespace api
{
	namespace v1
	{
		namespace stats
		{
			void StatsController::PrepareHandlers()
			{
				CreateSubController<CurrentController>(R"(\/current)");
			};
		}  // namespace stats
	}	   // namespace v1
}  // namespace api
