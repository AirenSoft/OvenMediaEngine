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
		class StreamsController : public Controller<StreamsController>
		{
		public:
			void PrepareHandlers() override;

		protected:
			// GET /v1/vhosts/<vhost_name>/apps/<app_name>/streams
			ApiResponse OnGetStreamList(const std::shared_ptr<HttpClient> &client);

			// GET /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>
			ApiResponse OnGetStream(const std::shared_ptr<HttpClient> &client);
		};
	}  // namespace v1
}  // namespace api
