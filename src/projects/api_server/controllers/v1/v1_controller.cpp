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
#include "vhosts/vhost_controller.h"

namespace api
{
	namespace v1
	{
		void V1Controller::PrepareHandlers()
		{
			AppendPrefix("/v1");

			CreateSubController<VHostController>();
			CreateSubController<StatsController>();
		};
	}  // namespace v1
}  // namespace api