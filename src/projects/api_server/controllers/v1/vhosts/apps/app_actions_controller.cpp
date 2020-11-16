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

			Json::Value response;
			std::vector<std::shared_ptr<info::Record>> records;
			
			auto error = publisher->GetRecords(app->GetName(), records);
			if(error->GetCode() != FilePublisher::FilePublisherStatusCode::Success)
			{
				return ov::Error::CreateError(HttpStatusCode::NoContent, error->GetMessage());
			}

			for ( auto &item : records )
			{
				response.append(conv::JsonFromRecord(item));
			}

			return response;
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

			auto record = conv::RecordFromJson(request_body);
			if (record == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}
			record->SetVhost(vhost->GetName().CStr());
			record->SetApplication(app->GetName().GetAppName());


			auto error = publisher->RecordStart(app->GetName(), record);
			if(error->GetCode() != FilePublisher::FilePublisherStatusCode::Success)
			{
				return ov::Error::CreateError(HttpStatusCode::NoContent, error->GetMessage());
			}

			return ov::Error::CreateError(HttpStatusCode::OK, error->GetMessage());
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

			auto record = conv::RecordFromJson(request_body);
			if (record == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}
			record->SetVhost(vhost->GetName().CStr());
			record->SetApplication(app->GetName().GetAppName());


			auto error = publisher->RecordStop(app->GetName(), record);
			if(error->GetCode() != FilePublisher::FilePublisherStatusCode::Success)
			{
				return ov::Error::CreateError(HttpStatusCode::NoContent, error->GetMessage());
			}

			return ov::Error::CreateError(HttpStatusCode::OK, error->GetMessage());
		}

		ApiResponse AppActionsController::OnGetPushes(const std::shared_ptr<HttpClient> &client,
													 const std::shared_ptr<mon::HostMetrics> &vhost,
													 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<RtmpPushPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::RtmpPush));
			if (publisher == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}

			Json::Value response;
			std::vector<std::shared_ptr<info::Push>> pushes;
			
			auto error = publisher->GetPushes(app->GetName(), pushes);
			if(error->GetCode() != RtmpPushPublisher::PushPublisherErrorCode::Success)
			{
				return ov::Error::CreateError(HttpStatusCode::NoContent, error->GetMessage());
			}

			for ( auto &item : pushes )
			{
				response.append(conv::JsonFromPush(item));
			}

			return response;
		}

		ApiResponse AppActionsController::OnPostStartPush(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
														  const std::shared_ptr<mon::HostMetrics> &vhost,
														  const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<RtmpPushPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::RtmpPush));
			if (publisher == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}

			auto push = conv::PushFromJson(request_body);
			if (push == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}
			push->SetVhost(vhost->GetName().CStr());
			push->SetApplication(app->GetName().GetAppName());

			// logtd("%s", push->GetInfoString().CStr());

			auto error = publisher->PushStart(app->GetName(), push);
			if(error->GetCode() != RtmpPushPublisher::PushPublisherErrorCode::Success)
			{
				return ov::Error::CreateError(HttpStatusCode::NoContent, error->GetMessage());
			}
			return ov::Error::CreateError(HttpStatusCode::OK, error->GetMessage());
		}

		ApiResponse AppActionsController::OnPostStopPush(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
														 const std::shared_ptr<mon::HostMetrics> &vhost,
														 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<RtmpPushPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::RtmpPush));
			if (publisher == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}

			auto push = conv::PushFromJson(request_body);
			if (push == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName());
			}
			push->SetVhost(vhost->GetName().CStr());
			push->SetApplication(app->GetName().GetAppName());

			// logte("%s", push->GetInfoString().CStr());

			auto error = publisher->PushStop(app->GetName(), push);
			if(error->GetCode() != RtmpPushPublisher::PushPublisherErrorCode::Success)
			{
				return ov::Error::CreateError(HttpStatusCode::NoContent, error->GetMessage());
			}

			return ov::Error::CreateError(HttpStatusCode::OK, error->GetMessage());;

		}

		ApiResponse AppActionsController::OnGetDummyAction(const std::shared_ptr<HttpClient> &client,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			logte("Called OnGetDummyAction. invoke [%s/%s]",
				  vhost->GetName().CStr(), app->GetName().GetAppName());

			return conv::JsonFromApplication(app);
		}

	}  // namespace v1
}  // namespace api
