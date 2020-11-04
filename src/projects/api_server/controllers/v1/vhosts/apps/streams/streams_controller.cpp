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

		ApiResponse StreamsController::OnGetStreamList(const std::shared_ptr<HttpClient> &client)
		{
			auto &match_result = client->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			auto app_name = match_result.GetNamedGroup("app_name");
			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}

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

		ApiResponse StreamsController::OnGetStream(const std::shared_ptr<HttpClient> &client)
		{
			auto &match_result = client->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			auto app_name = match_result.GetNamedGroup("app_name");
			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}

			auto stream_name = match_result.GetNamedGroup("stream_name");
			std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams;
			auto stream = GetStream(app, stream_name, &output_streams);
			if (stream == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find stream: [%.*s/%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data(),
											  stream_name.length(), stream_name.data());
			}

			return api::conv::JsonFromStream(stream, std::move(output_streams));
		}
	}  // namespace v1
}  // namespace api
