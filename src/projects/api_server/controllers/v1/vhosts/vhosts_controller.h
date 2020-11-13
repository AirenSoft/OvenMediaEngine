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
		class VHostsController : public Controller<VHostsController>
		{
		public:
			void PrepareHandlers() override;

			// GET /v1/vhosts
			ApiResponse OnGetVhostList(const std::shared_ptr<HttpClient> &client);

			// GET /v1/vhosts/<vhost_name>
			ApiResponse OnGetVhost(const std::shared_ptr<HttpClient> &client,
								   const std::shared_ptr<mon::HostMetrics> &vhost);
		};
	}  // namespace v1
}  // namespace api
