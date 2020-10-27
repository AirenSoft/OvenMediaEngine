//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../controller.h"

namespace api
{
	namespace v1
	{
		// /v<version>/vhosts
		// /v<version>/vhosts/<vhost_name>
		class StatsController : public Controller<StatsController>
		{
		public:
			void PrepareHandlers() override;
		};
	}  // namespace v1
}  // namespace api
