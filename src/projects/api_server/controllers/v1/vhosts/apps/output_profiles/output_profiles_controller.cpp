//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "output_profiles_controller.h"

#include <orchestrator/orchestrator.h>

#include <functional>

#include "../../../../../api_private.h"
#include "../../../../../converters/converters.h"
#include "../../../../../helpers/helpers.h"

namespace api
{
	namespace v1
	{
		void OutputProfilesController::PrepareHandlers()
		{
			RegisterPost(R"()", &OutputProfilesController::OnPostStream);
			RegisterGet(R"()", &OutputProfilesController::OnGetStreamList);
			RegisterGet(R"(\/(?<stream_name>[^\/]*))", &OutputProfilesController::OnGetStream);
			RegisterDelete(R"(\/(?<stream_name>[^\/]*))", &OutputProfilesController::OnDeleteStream);
		};

		ApiResponse OutputProfilesController::OnPostStream(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			if (request_body.isArray() == false)
			{
				return ov::Error::CreateError(HttpStatusCode::BadRequest, "Request body must be an array");
			}

			auto orchestrator = ocst::Orchestrator::GetInstance();
			Json::Value response_value(Json::ValueType::arrayValue);
			auto url = ov::Url::Parse(client->GetRequest()->GetUri());

			for (auto &item : request_body)
			{
				auto stream_name = item["name"].asCString();
				auto result = orchestrator->RequestPullStream(
					url, app->GetName(),
					stream_name, item["url"].asCString());

				if (result)
				{
					std::shared_ptr<mon::StreamMetrics> stream;
					std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams;

					GetStream(app, stream_name, &output_streams);
				}
				else
				{
					Json::Value error_value;
					error_value["message"] = "Could not pull the stream";
					response_value.append(error_value);
				}
			}

			return response_value;
		}

		ApiResponse OutputProfilesController::OnGetStreamList(const std::shared_ptr<HttpClient> &client,
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

		ApiResponse OutputProfilesController::OnGetStream(const std::shared_ptr<HttpClient> &client,
														  const std::shared_ptr<mon::HostMetrics> &vhost,
														  const std::shared_ptr<mon::ApplicationMetrics> &app,
														  const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			return api::conv::JsonFromStream(stream, std::move(output_streams));
		}

		ApiResponse OutputProfilesController::OnDeleteStream(const std::shared_ptr<HttpClient> &client,
															 const std::shared_ptr<mon::HostMetrics> &vhost,
															 const std::shared_ptr<mon::ApplicationMetrics> &app,
															 const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			// auto orchestrator = ocst::Orchestrator::GetInstance();

			return {};
		}
	}  // namespace v1
}  // namespace api
