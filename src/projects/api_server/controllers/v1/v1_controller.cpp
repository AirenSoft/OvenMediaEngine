//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "v1_controller.h"

#include "stats/stats_controller.h"
#include "vhosts/vhosts_controller.h"

namespace api
{
	namespace v1
	{
		void V1Controller::PrepareHandlers()
		{
			CreateSubController<VHostsController>(R"(\/vhosts)");
			
			CreateSubController<stats::StatsController>(R"(\/stats)");
		};
	}  // namespace v1
}  // namespace api