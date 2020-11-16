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

	protected:
		ApiResponse OnNotFound(const std::shared_ptr<HttpClient> &client);
	};
}  // namespace api