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
		class VHostController : public Controller<VHostController>
		{
		public:
			void PrepareHandlers() override;

			// GET /v1/vhosts
			HttpNextHandler OnGetVhostList(const std::shared_ptr<HttpClient> &client);

			// GET /v1/vhosts/<vhost_name>
			HttpNextHandler OnGetVhost(const std::shared_ptr<HttpClient> &client);
		};
	}  // namespace v1
}  // namespace api
