//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "app_actions_controller.h"

#include <orchestrator/orchestrator.h>

#include <functional>

#include "../../../../api_private.h"
#include "../../../../converters/converters.h"
#include "../../../../helpers/helpers.h"
#include "publishers/publishers.h"

namespace api
{
	namespace v1
	{
		void AppActionsController::PrepareHandlers()
		{
			RegisterGet(R"()", &AppActionsController::OnGetDummyAction);

			// Record related actions
			RegisterGet(R"((records))", &AppActionsController::OnGetRecords);
			RegisterPost(R"((startRecord))", &AppActionsController::OnPostStartRecord);
			RegisterPost(R"((stopRecord))", &AppActionsController::OnPostStopRecord);

			// Push related actions
			RegisterGet(R"((pushes))", &AppActionsController::OnGetPushes);
			RegisterPost(R"((startPush))", &AppActionsController::OnPostStartPush);
			RegisterPost(R"((stopPush))", &AppActionsController::OnPostStopPush);
		};

		ApiResponse AppActionsController::OnGetRecords(const std::shared_ptr<HttpClient> &client,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}

			std::vector<std::shared_ptr<info::Record>> record_list;

			auto error = publisher->GetRecords(app->GetName(), record_list);

			// for ( auto &item : record_list )
			// {
			// 	logtd("\n%s", item->GetInfoString().CStr());
			// }

			return error;
		}

		ApiResponse AppActionsController::OnPostStartRecord(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
															const std::shared_ptr<mon::HostMetrics> &vhost,
															const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			// logte("JsonContext : %s", ov::Json::Stringify(request_body).CStr());

			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}

			auto record = api::conv::RecordFromJson(request_body);
			if (record == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}

			auto error = publisher->RecordStart(app->GetName(), record);

			return error;
		}

		ApiResponse AppActionsController::OnPostStopRecord(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			// logte("JsonContext : %s", ov::Json::Stringify(request_body).CStr());

			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}

			auto record = api::conv::RecordFromJson(request_body);
			if (record == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}

			auto error = publisher->RecordStop(app->GetName(), record);

			return error;
		}

		ApiResponse AppActionsController::OnGetPushes(const std::shared_ptr<HttpClient> &client)
		{
			Json::Value response(Json::ValueType::objectValue);
			return std::move(response);
		}

		ApiResponse AppActionsController::OnPostStartPush(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body)
		{
			Json::Value response(Json::ValueType::objectValue);
			return std::move(response);
		}

		ApiResponse AppActionsController::OnPostStopPush(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body)
		{
			Json::Value response(Json::ValueType::objectValue);
			return std::move(response);
		}

		ApiResponse AppActionsController::OnGetDummyAction(const std::shared_ptr<HttpClient> &client,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			logte("Called OnGetDummyAction. invoke [%s/%s]",
				  vhost->GetName().CStr(), app->GetName().GetAppName());

			return api::conv::JsonFromApplication(app);
		}

		ApiResponse AppActionsController::OnPostDummyAction(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
															const std::shared_ptr<mon::HostMetrics> &vhost,
															const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			logte("Called OnPostDummyAction. invoke [%s/%s]",
				  vhost->GetName().CStr(), app->GetName().GetAppName());

			return api::conv::JsonFromApplication(app);
		}

	}  // namespace v1
}  // namespace api
