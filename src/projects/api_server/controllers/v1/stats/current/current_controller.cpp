//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "current_controller.h"

#include "vhosts/vhosts_controller.h"

namespace api
{
	namespace v1
	{
		namespace stats
		{
			void CurrentController::PrepareHandlers()
			{
				CreateSubController<VHostsController>(R"(\/vhosts)");
			};
		}  // namespace stats
	}	   // namespace v1
}  // namespace api
