//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "streams_controller.h"

#include <orchestrator/orchestrator.h>

#include <functional>

#include "../../../../../api_private.h"
#include "../../../../../converters/converters.h"
#include "../../../../../helpers/helpers.h"

namespace api
{
	namespace v1
	{
		void StreamsController::PrepareHandlers()
		{
			RegisterGet(R"()", &StreamsController::OnGetStreamList);
			RegisterGet(R"(\/(?<stream_name>[^\/]*))", &StreamsController::OnGetStream);
		};

		ApiResponse StreamsController::OnGetStreamList(const std::shared_ptr<HttpClient> &client,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response = Json::arrayValue;

			auto stream_list = app->GetStreamMetricsList();

			for (auto &item : stream_list)
			{
				auto &stream = item.second;

				if (stream->GetOriginStream() == nullptr)
				{
					response.append(stream->GetName().CStr());
				}
			}

			return response;
		}

		ApiResponse StreamsController::OnGetStream(const std::shared_ptr<HttpClient> &client,
												   const std::shared_ptr<mon::HostMetrics> &vhost,
												   const std::shared_ptr<mon::ApplicationMetrics> &app,
												   const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			return api::conv::JsonFromStream(stream, std::move(output_streams));
		}
	}  // namespace v1
}  // namespace api
