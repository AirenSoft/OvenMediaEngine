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
#include <base/provider/pull_provider/stream_props.h>
#include <functional>

#include "stream_actions_controller.h"
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

			CreateSubController<StreamActionsController>(R"(\/(?<stream_name>[^\/:]*):)");
		};

		ApiResponse StreamsController::OnPostStream(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
													const std::shared_ptr<mon::HostMetrics> &vhost,
													const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams;

			auto orchestrator = ocst::Orchestrator::GetInstance();
			auto source_url = client->GetRequest()->GetParsedUri();

			if (request_body.isObject() == false)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Request body must be an object");
			}

			auto jv_stream_name = request_body["name"];
			auto jv_urls = request_body["urls"];
			auto jv_properties = request_body["properties"];

			if (jv_stream_name.isNull() || jv_stream_name.isString() == false ||
				jv_urls.isNull() || jv_urls.isArray() == false)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Stream name or urls is required");
			}

			if (jv_urls.size() == 0)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Urls must be at least one");
			}

			ov::String stream_name = jv_stream_name.asCString();
			std::vector<ov::String> request_urls;

			for (auto &jv_url : jv_urls)
			{
				if (jv_url.isString() == false)
				{
					throw http::HttpError(http::StatusCode::BadRequest, "Url must be a string");
				}

				request_urls.push_back(jv_url.asCString());
			}

			auto stream = GetStream(app, stream_name, nullptr);
			try
			{
				if (stream == nullptr)
				{
					auto properties = std::make_shared<pvd::PullStreamProperties>();

					if (jv_properties.isNull() == false && jv_properties.isObject())
					{
						if (jv_properties["persistent"].isNull() == false && jv_properties["persistent"].isBool())
						{
							properties->EnablePersistent(jv_properties["persistent"].asBool());
						}

						if (jv_properties["noInputFailoverTimeoutMs"].isNull() == false && jv_properties["noInputFailoverTimeoutMs"].isInt())
						{
							properties->SetNoInputFailoverTimeout(jv_properties["noInputFailoverTimeoutMs"].asInt());
						}

						if (jv_properties["unusedStreamDeletionTimeoutMs"].isNull() == false && jv_properties["unusedStreamDeletionTimeoutMs"].isInt())
						{
							properties->SetUnusedStreamDeletionTimeout(jv_properties["unusedStreamDeletionTimeoutMs"].asInt());
						}

						if (jv_properties["ignoreRtcpSRTimestamp"].isNull() == false && jv_properties["ignoreRtcpSRTimestamp"].isBool())
						{
							properties->EnableIgnoreRtcpSRTimestamp(jv_properties["ignoreRtcpSRTimestamp"].asBool());
						}
					}
					
					logti("Request to pull stream: %s/%s - persistent(%s) noInputFailoverTimeoutMs(%d) unusedStreamDeletionTimeoutMs(%d) ignoreRtcpSRTimestamp(%s)", app->GetName().CStr(), stream_name.CStr(), properties->IsPersistent() ? "true" : "false", properties->GetNoInputFailoverTimeout(), properties->GetUnusedStreamDeletionTimeout(), properties->IsRtcpSRTimestampIgnored() ? "true" : "false");
					for (auto &url : request_urls)
					{
						logti(" - %s", url.CStr());
					}

					auto result = orchestrator->RequestPullStreamWithUrls(source_url, app->GetName(), stream_name, request_urls, 0, properties);

					if (result)
					{
						std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams;
						stream = GetStream(app, stream_name, &output_streams);
						if (stream == nullptr)
						{
							throw http::HttpError(http::StatusCode::BadGateway, ov::String::FormatString("Could not pull the stream : %s", source_url->ToUrlString(true).CStr()));
						}

						return {http::StatusCode::Created};
					}
					else
					{
						throw http::HttpError(http::StatusCode::BadGateway, ov::String::FormatString("Could not pull the stream : %s", source_url->ToUrlString(true).CStr()));
					}
				}
				else
				{
					throw http::HttpError(http::StatusCode::Conflict, "Stream already exists");
				}
			}
			catch (const http::HttpError &error)
			{
				throw error;
			}

			return {http::StatusCode::InternalServerError};
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
			// Update llhls playlist from the LLHLS stream
			auto orchestrator = ocst::Orchestrator::GetInstance();
			auto app_name = app->GetName();
			auto stream_name = stream->GetName();

			auto publisher = orchestrator->GetPublisherFromType(PublisherType::LLHls);
			if (publisher)
			{
				
				for (auto &output_stream : output_streams)
				{
					auto llhls_stream = publisher->GetStream(app->GetId(), output_stream->GetId());
					if (llhls_stream)
					{
						auto llhls_playlist = llhls_stream->GetPlaylist("llhls");
						if (llhls_playlist)
						{
							output_stream->AddPlaylist(std::make_shared<info::Playlist>(*llhls_playlist));
						}
					}
				}				
			}

			// update webrtc_default playlist from the WebRTC stream
			publisher = orchestrator->GetPublisherFromType(PublisherType::Webrtc);
			if (publisher)
			{
				for (auto &output_stream : output_streams)
				{
					auto webrtc_stream = publisher->GetStream(app->GetId(), output_stream->GetId());
					if (webrtc_stream)
					{
						auto webrtc_playlist = webrtc_stream->GetPlaylist("webrtc_default");
						if (webrtc_playlist)
						{
							output_stream->AddPlaylist(std::make_shared<info::Playlist>(*webrtc_playlist));
						}
					}
				}
			}

			return ::serdes::JsonFromStream(stream, std::move(output_streams));
		}

		ApiResponse StreamsController::OnDeleteStream(const std::shared_ptr<http::svr::HttpExchange> &client,
													  const std::shared_ptr<mon::HostMetrics> &vhost,
													  const std::shared_ptr<mon::ApplicationMetrics> &app,
													  const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			auto orchestrator = ocst::Orchestrator::GetInstance();

			auto app_name = app->GetName();
			auto stream_name = stream->GetName();

			auto code = orchestrator->TerminateStream(app_name, stream_name);
			auto http_code = http::StatusCodeFromCommonError(code);
			if (http_code != http::StatusCode::OK)
			{	
				throw http::HttpError(http_code, "Could not terminate the stream");
			}

			return {http_code};
		}
	}  // namespace v1
}  // namespace api
