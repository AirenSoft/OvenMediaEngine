//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../controller_base.h"

namespace api
{
	namespace v1
	{
		namespace stats
		{
			class StatsController : public ControllerBase<StatsController>
			{
			public:
				void PrepareHandlers() override;
			};
		}  // namespace stats
	}  // namespace v1
}  // namespace api
