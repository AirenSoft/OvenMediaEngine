//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stream_actions_controller.h"
#include "../../../../../api_private.h"

namespace api
{
	namespace v1
	{
		void StreamActionsController::PrepareHandlers()
		{
			RegisterGet(R"()", &StreamActionsController::OnGetDummyAction);

			RegisterPost(R"((hlsDumps))", &StreamActionsController::OnPostHLSDumps);
			RegisterPost(R"((startHlsDump))", &StreamActionsController::OnPostStartHLSDump);
			RegisterPost(R"((stopHlsDump))", &StreamActionsController::OnPostStopHLSDump);
		}

		// POST /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>:hlsDumps
		// {
		//  "outputStreamName": "stream",
		//  "id": "dump_id"
		// }
		ApiResponse StreamActionsController::OnPostHLSDumps(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
									const std::shared_ptr<mon::HostMetrics> &vhost,
									const std::shared_ptr<mon::ApplicationMetrics> &app,
									const std::shared_ptr<mon::StreamMetrics> &stream,
									const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			return {http::StatusCode::InternalServerError, "Not implemented"};
		}

		// POST /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>:startHlsDump
		// {
		// 	"outputStreamName": "stream",
		// 	"id": "dump_id",
		// 	"outputPath": "/tmp/",
		// 	"playlist" : ["llhls.m3u8", "abr.m3u8"],
		// 	"userData":{
		// 		"fileName" : "s3.info",
		// 		"fileData" : "s3://id:password@bucket_name/obj_name"
		//	}
		// }
		ApiResponse StreamActionsController::OnPostStartHLSDump(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
										const std::shared_ptr<mon::HostMetrics> &vhost,
										const std::shared_ptr<mon::ApplicationMetrics> &app,
										const std::shared_ptr<mon::StreamMetrics> &stream,
										const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			auto dump_info = ::serdes::DumpInfoFromJson(request_body);
			if (dump_info == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr(), stream->GetName().CStr());
			}

			if (dump_info->GetId().IsEmpty() == true || 
				dump_info->GetStreamName().IsEmpty() == true ||
				dump_info->GetOutputPath().IsEmpty() == true)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "id, outputStreamName and outputPath are required");
			}

			auto output_stream_it = std::find_if(output_streams.begin(), output_streams.end(), [&](const auto &output_stream) {
				return output_stream->GetName() == dump_info->GetStreamName();
			});

			if (output_stream_it == output_streams.end())
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find output stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			auto output_stream = *output_stream_it;
			auto llhls_stream = GetLLHlsStream(output_stream);
			if (llhls_stream == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find LLHLS stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			auto [result, reason] = llhls_stream->StartDump(dump_info);
			if (result == false)
			{
				throw http::HttpError(http::StatusCode::InternalServerError,
									  "Could not start dump: [%s/%s/%s] (%s)",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr(), dump_info->GetStreamName().CStr(), reason.CStr());
			}

			return {http::StatusCode::OK};
		}

		// POST /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>:stopHlsDump
		// {
		//  "outputStreamName": "stream",
		//  "id": "dump_id"
		// }
		ApiResponse StreamActionsController::OnPostStopHLSDump(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
										const std::shared_ptr<mon::HostMetrics> &vhost,
										const std::shared_ptr<mon::ApplicationMetrics> &app,
										const std::shared_ptr<mon::StreamMetrics> &stream,
										const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			auto dump_info = ::serdes::DumpInfoFromJson(request_body);
			if (dump_info == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr(), stream->GetName().CStr());
			}

			if (dump_info->GetId().IsEmpty() == true || 
				dump_info->GetStreamName().IsEmpty() == true)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "id, outputStreamName and outputPath are required");
			}

			auto output_stream_it = std::find_if(output_streams.begin(), output_streams.end(), [&](const auto &output_stream) {
				return output_stream->GetName() == dump_info->GetStreamName();
			});

			if (output_stream_it == output_streams.end())
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find output stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			auto output_stream = *output_stream_it;
			auto llhls_stream = GetLLHlsStream(output_stream);
			if (llhls_stream == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find LLHLS stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			auto [result, reason] = llhls_stream->StopDump(dump_info);
			if (result == false)
			{
				throw http::HttpError(http::StatusCode::InternalServerError,
									  "Could not stop dump: [%s/%s/%s] (%s)",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr(), dump_info->GetStreamName().CStr(), reason.CStr());
			}

			return {http::StatusCode::OK};
		}

		ApiResponse StreamActionsController::OnGetDummyAction(const std::shared_ptr<http::svr::HttpExchange> &client,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app,
														   const std::shared_ptr<mon::StreamMetrics> &stream,
														   const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			logte("Called OnGetDummyAction. invoke [%s/%s/%s]", vhost->GetName().CStr(), app->GetName().GetAppName(), stream->GetName().CStr());

			return app->GetConfig().ToJson();
		}

		std::shared_ptr<LLHlsStream> StreamActionsController::GetLLHlsStream(const std::shared_ptr<mon::StreamMetrics> &stream_metric)
		{
			auto publisher = std::dynamic_pointer_cast<LLHlsPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::LLHls));
			if (publisher == nullptr)
			{
				return nullptr;
			}

			auto appliation = publisher->GetApplicationByName(stream_metric->GetApplicationInfo().GetName());
			if (appliation == nullptr)
			{
				return nullptr;
			}

			auto stream = appliation->GetStream(stream_metric->GetName());
			if (stream == nullptr)
			{
				return nullptr;
			}

			return std::dynamic_pointer_cast<LLHlsStream>(stream);
		}
	}
}