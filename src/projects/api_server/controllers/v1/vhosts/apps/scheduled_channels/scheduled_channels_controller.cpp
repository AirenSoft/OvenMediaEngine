//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "scheduled_channels_controller.h"

#include <orchestrator/orchestrator.h>
#include "../../../../../api_private.h"
#include <providers/scheduled/scheduled_stream.h>
#include <providers/scheduled/schedule.h>
#include <base/ovlibrary/files.h>

namespace api
{
	namespace v1
	{
		void ScheduledChannelsController::PrepareHandlers()
		{
			RegisterGet(R"()", &ScheduledChannelsController::OnGetChannelList);
			RegisterPost(R"()", &ScheduledChannelsController::OnCreateChannel);
			RegisterGet(R"(\/(?<stream_name>[^\/]*))", &ScheduledChannelsController::OnGetChannel);
			RegisterPut(R"(\/(?<stream_name>[^\/]*))", &ScheduledChannelsController::OnPutChannel);
			RegisterDelete(R"(\/(?<stream_name>[^\/]*))", &ScheduledChannelsController::OnDeleteChannel);
		};

		ApiResponse ScheduledChannelsController::OnGetChannelList(const std::shared_ptr<http::svr::HttpExchange> &client,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response = Json::arrayValue;

			auto stream_list = app->GetStreamMetricsMap();

			for (auto &item : stream_list)
			{
				auto &stream = item.second;

				if (stream->GetSourceType() != StreamSourceType::Scheduled)
				{
					continue;
				}

				if (stream->GetLinkedInputStream() == nullptr)
				{
					response.append(stream->GetName().CStr());
				}
			}

			return response;
		}


		ApiResponse ScheduledChannelsController::OnCreateChannel(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
													const std::shared_ptr<mon::HostMetrics> &vhost,
													const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			// Validation check
			if (!request_body.isObject())
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Invalid request body");
			}

			// Get Config
			auto schedule_config = app->GetConfig().GetProviders().GetScheduledProvider();
			if (schedule_config.IsParsed() == false)
			{
				return {http::StatusCode::Forbidden}; // Not enabled
			}

			auto media_root_dir = ov::GetAbsolutePath(schedule_config.GetMediaRootDir());
			auto schedule_files_dir = ov::GetAbsolutePath(schedule_config.GetScheduleFilesDir());

			auto [schedule, error] = pvd::Schedule::CreateFromJsonObject(request_body, media_root_dir);
			if (schedule == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest, error);
			}

			// Check if stream is already exists
			auto stream_name = schedule->GetStream().name;
			
			auto stream = GetStream(app, stream_name, nullptr);
			if (stream != nullptr)
			{
				throw http::HttpError(http::StatusCode::Conflict, "Stream already exists");
			}

			auto schedule_file_path = ov::String::FormatString("%s/%s.%s", schedule_files_dir.CStr(), schedule->GetStream().name.CStr(), pvd::ScheduleFileExtension);

			auto error_code = schedule->SaveToXMLFile(schedule_file_path);
			if (error_code != CommonErrorCode::SUCCESS)
			{
				throw http::HttpError(http::StatusCode::InternalServerError, "Could not save the schedule file");
			}
			
			return {http::StatusCode::Created};
		}

		// PUT /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>
		ApiResponse ScheduledChannelsController::OnPutChannel(const std::shared_ptr<http::svr::HttpExchange> &client,
									const Json::Value &request_body,
									const std::shared_ptr<mon::HostMetrics> &vhost,
									const std::shared_ptr<mon::ApplicationMetrics> &app,
									const std::shared_ptr<mon::StreamMetrics> &stream, 
									const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			// Validation check
			if (!request_body.isObject())
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Invalid request body");
			}

			if (stream->GetSourceType() != StreamSourceType::Scheduled)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Stream is not a scheduled stream");
			}

			// Get Real ScheduledStream 
			auto provider = std::dynamic_pointer_cast<pvd::Provider>(ocst::Orchestrator::GetInstance()->GetProviderFromType(ProviderType::Scheduled));
			if (provider == nullptr)
			{
				throw http::HttpError(http::StatusCode::InternalServerError, "Could not get the scheduled provider");
			}

			auto scheduled_stream = std::static_pointer_cast<pvd::ScheduledStream>(provider->GetStreamByName(app->GetName(), stream->GetName()));
			if (scheduled_stream == nullptr)
			{
				throw http::HttpError(http::StatusCode::InternalServerError, "Could not get the scheduled stream");
			}

			// Get Config
			auto schedule_config = app->GetConfig().GetProviders().GetScheduledProvider();
			if (schedule_config.IsParsed() == false)
			{
				return {http::StatusCode::Forbidden}; // Not enabled
			}

			auto media_root_dir = ov::GetAbsolutePath(schedule_config.GetMediaRootDir());
			auto schedule_files_dir = ov::GetAbsolutePath(schedule_config.GetScheduleFilesDir());

			auto [schedule, error] = pvd::Schedule::CreateFromJsonObject(request_body, media_root_dir);
			if (schedule == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest, error);
			}

			if (stream->GetName() != schedule->GetStream().name)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Stream name is not matched with the request body");
			}

			auto schedule_file_path = ov::String::FormatString("%s/%s.%s", schedule_files_dir.CStr(), schedule->GetStream().name.CStr(), pvd::ScheduleFileExtension);

			auto error_code = schedule->SaveToXMLFile(schedule_file_path);
			if (error_code != CommonErrorCode::SUCCESS)
			{
				throw http::HttpError(http::StatusCode::InternalServerError, "Could not save the schedule file");
			}
			
			return {http::StatusCode::Created};
		}
		
		ApiResponse ScheduledChannelsController::OnGetChannel(const std::shared_ptr<http::svr::HttpExchange> &client,
												   const std::shared_ptr<mon::HostMetrics> &vhost,
												   const std::shared_ptr<mon::ApplicationMetrics> &app,
												   const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			// Get Real ScheduledStream
			auto provider = std::dynamic_pointer_cast<pvd::Provider>(ocst::Orchestrator::GetInstance()->GetProviderFromType(ProviderType::Scheduled));
			if (provider == nullptr)
			{
				throw http::HttpError(http::StatusCode::InternalServerError, "Could not get the scheduled provider");
			}

			auto scheduled_stream = std::static_pointer_cast<pvd::ScheduledStream>(provider->GetStreamByName(app->GetName(), stream->GetName()));
			if (scheduled_stream == nullptr)
			{
				throw http::HttpError(http::StatusCode::InternalServerError, "Could not get the scheduled stream");
			}

			auto schedule = scheduled_stream->CopySchedule();
			if (schedule == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound, "Could not get the schedule");
			}

			Json::Value response;
			if (schedule->ToJsonObject(response) != CommonErrorCode::SUCCESS)
			{
				throw http::HttpError(http::StatusCode::InternalServerError, "Could not convert the schedule to json");
			}

			auto [program, item] = scheduled_stream->GetCurrentProgram();
			if (program != nullptr)
			{
				Json::Value curr_program_json;

				curr_program_json["name"] = program->name.CStr();
				curr_program_json["scheduled"] = ov::Converter::ToISO8601String(program->scheduled_time).CStr();
				curr_program_json["end"] = ov::Converter::ToISO8601String(program->end_time).CStr();
				curr_program_json["duration"] = program->duration_ms;
				curr_program_json["repeat"] = program->repeat;

				if (item != nullptr)
				{
					Json::Value item_json;
					item_json["url"] = item->url.CStr();
					item_json["duration"] = item->duration_ms;
					item_json["start"] = item->start_time_ms;

					curr_program_json["currentItem"] = item_json;
				}

				response["currentProgram"] = curr_program_json;
			}

			return response;
		}

		ApiResponse ScheduledChannelsController::OnDeleteChannel(const std::shared_ptr<http::svr::HttpExchange> &client,
													  const std::shared_ptr<mon::HostMetrics> &vhost,
													  const std::shared_ptr<mon::ApplicationMetrics> &app,
													  const std::shared_ptr<mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			if (stream->GetSourceType() != StreamSourceType::Scheduled)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Stream is not a scheduled stream");
			}

			// Get Config
			auto schedule_config = app->GetConfig().GetProviders().GetScheduledProvider();
			if (schedule_config.IsParsed() == false)
			{
				return {http::StatusCode::Forbidden}; // Not enabled
			}

			auto media_root_dir = ov::GetAbsolutePath(schedule_config.GetMediaRootDir());
			auto schedule_files_dir = ov::GetAbsolutePath(schedule_config.GetScheduleFilesDir());

			auto schedule_file_path = ov::String::FormatString("%s/%s.%s", schedule_files_dir.CStr(), stream->GetName().CStr(), pvd::ScheduleFileExtension);

			// Delete schedule file
			if (ov::DeleteFile(schedule_file_path) == false)
			{
				throw http::HttpError(http::StatusCode::InternalServerError, "Could not delete the schedule file");
			}

			return {http::StatusCode::OK};
		}
	}  // namespace v1
}  // namespace api
