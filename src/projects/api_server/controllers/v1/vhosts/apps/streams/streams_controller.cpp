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

namespace api
{
	namespace v1
	{
		void StreamsController::PrepareHandlers()
		{
			RegisterPost(R"()", &StreamsController::OnPostStream);
			RegisterGet(R"()", &StreamsController::OnGetStreamList);
			RegisterGet(R"(\/(?<stream_name>[^\/]*))", &StreamsController::OnGetStream);
			RegisterDelete(R"(\/(?<stream_name>[^\/]*))", &StreamsController::OnDeleteStream);
		};

		ApiResponse StreamsController::OnPostStream(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
													const std::shared_ptr<mon::HostMetrics> &vhost,
													const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			if (request_body.isArray() == false)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Request body must be an array");
			}

			std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams;

			auto orchestrator = ocst::Orchestrator::GetInstance();
			Json::Value response_value(Json::ValueType::arrayValue);
			auto url = ov::Url::Parse(client->GetRequest()->GetUri());

			MultipleStatus status_codes;

			for (auto &item : request_body)
			{
				auto stream_name = item["name"].asCString();
				auto stream = GetStream(app, stream_name, nullptr);

				try
				{
					if (stream == nullptr)
					{
						auto result = orchestrator->RequestPullStream(
							url, app->GetName(),
							stream_name, item["url"].asCString());

						if (result)
						{
							std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams;
							stream = GetStream(app, stream_name, &output_streams);

							response_value.append(::serdes::JsonFromStream(stream, std::move(output_streams)));
						}
						else
						{
							throw http::HttpError(http::StatusCode::InternalServerError, "Could not pull the stream");
						}
					}
					else
					{
						throw http::HttpError(http::StatusCode::Conflict, "Stream already exists");
					}
				}
				catch (const http::HttpError &error)
				{
					status_codes.AddStatusCode(error.GetStatusCode());
					response_value.append(::serdes::JsonFromError(error));
				}
			}

			return response_value;
		}

		ApiResponse StreamsController::OnGetStreamList(const std::shared_ptr<http::svr::HttpExchange> &client,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response = Json::arrayValue;

			auto stream_list = app->GetStreamMetricsMap();

			for (auto &item : stream_list)
			{
				auto &stream = item.second;

				if (stream->GetLinkedInputStream() == nullptr)
				{
					response.append(stream->GetName().CStr());
				}
			}

			return response;
		}

		ApiResponse StreamsController::OnGetStream(const std::shared_ptr<http::svr::HttpExchange> &client,
												   const std::shared_ptr<mon::HostMetrics> &vhost,
												   const std::shared_ptr<mon::ApplicationMetrics> &app,
												   const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			return ::serdes::JsonFromStream(stream, std::move(output_streams));
		}

		ApiResponse StreamsController::OnDeleteStream(const std::shared_ptr<http::svr::HttpExchange> &client,
													  const std::shared_ptr<mon::HostMetrics> &vhost,
													  const std::shared_ptr<mon::ApplicationMetrics> &app,
													  const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			return http::StatusCode::NotImplemented;
		}
	}  // namespace v1
}  // namespace api
