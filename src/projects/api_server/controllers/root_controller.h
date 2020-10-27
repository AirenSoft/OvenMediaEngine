//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "controller.h"

namespace api
{
	class RootController : public Controller<RootController>
	{
	public:
		void PrepareHandlers() override;
	};
}  // namespace api