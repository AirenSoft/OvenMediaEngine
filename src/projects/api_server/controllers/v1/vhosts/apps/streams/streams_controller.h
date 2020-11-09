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
			ApiResponse OnGetStreamList(const std::shared_ptr<HttpClient> &client,
										const std::shared_ptr<mon::HostMetrics> &vhost,
										const std::shared_ptr<mon::ApplicationMetrics> &app);

			// GET /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>
			ApiResponse OnGetStream(const std::shared_ptr<HttpClient> &client,
									const std::shared_ptr<mon::HostMetrics> &vhost,
									const std::shared_ptr<mon::ApplicationMetrics> &app,
									const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams);
		};
	}  // namespace v1
}  // namespace api
