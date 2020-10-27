//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "root_controller.h"

#include "v1/v1_controller.h"

namespace api
{
	void RootController::PrepareHandlers()
	{
		// Currently only v1 is supported
		CreateSubController<v1::V1Controller>();
	};
}  // namespace api