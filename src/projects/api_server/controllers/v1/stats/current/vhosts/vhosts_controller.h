//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../../../controller.h"

namespace api
{
	namespace v1
	{
		namespace stats
		{
			class VHostsController : public Controller<VHostsController>
			{
			public:
				void PrepareHandlers() override;

			protected:
				ApiResponse OnGetVhost(const std::shared_ptr<HttpClient> &client,
									   const std::shared_ptr<mon::HostMetrics> &vhost);
			};
		}  // namespace stats
	}	   // namespace v1
}  // namespace api
