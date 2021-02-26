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
			// @GET action will be deprecated
			RegisterGet(R"((records))", &AppActionsController::OnGetRecords);
			RegisterPost(R"((records))", &AppActionsController::OnPostRecords);
			RegisterPost(R"((startRecord))", &AppActionsController::OnPostStartRecord);
			RegisterPost(R"((stopRecord))", &AppActionsController::OnPostStopRecord);

			// Push related actions
			// @GET action will be deprecated
			RegisterGet(R"((pushes))", &AppActionsController::OnGetPushes);
			RegisterPost(R"((pushes))", &AppActionsController::OnPostPushes);
			RegisterPost(R"((startPush))", &AppActionsController::OnPostStartPush);
			RegisterPost(R"((stopPush))", &AppActionsController::OnPostStopPush);
		};

		ApiResponse AppActionsController::OnGetRecords(const std::shared_ptr<HttpClient> &client,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value empty_body;

			return OnPostRecords(client, empty_body, vhost, app);
		}

		ApiResponse AppActionsController::OnPostRecords(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
														const std::shared_ptr<mon::HostMetrics> &vhost,
														const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			std::vector<std::shared_ptr<info::Record>> records;

			auto error = publisher->GetRecords(app->GetName(), records);
			if (error->GetCode() != FilePublisher::FilePublisherStatusCode::Success || records.size() == 0)
			{
				return HttpError::CreateError(HttpStatusCode::NoContent, "There is no record information");
			}

			Json::Value response;

			for (auto &item : records)
			{
				response.append(conv::JsonFromRecord(item));
			}

			return {HttpStatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStartRecord(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
															const std::shared_ptr<mon::HostMetrics> &vhost,
															const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			auto record = conv::RecordFromJson(request_body);
			if (record == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}
			else
			{
				record->SetVhost(vhost->GetName().CStr());
				record->SetApplication(app->GetName().GetAppName());
			}

			auto error = publisher->RecordStart(app->GetName(), record);
			if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureInvalidParameter)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, error->GetMessage());
			}
			else if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureDupulicateKey)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, error->GetMessage());
			}

			Json::Value response;
			response.append(conv::JsonFromRecord(record));

			return {HttpStatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStopRecord(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			auto record = conv::RecordFromJson(request_body);
			if (record == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}
			else
			{
				record->SetVhost(vhost->GetName().CStr());
				record->SetApplication(app->GetName().GetAppName());
			}

			auto error = publisher->RecordStop(app->GetName(), record);
			if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureInvalidParameter)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, error->GetMessage());
			}
			else if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureNotExist)
			{
				return HttpError::CreateError(HttpStatusCode::NotFound, error->GetMessage());
			}

			Json::Value response;
			response.append(conv::JsonFromRecord(record));

			return {HttpStatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnGetPushes(const std::shared_ptr<HttpClient> &client,
													  const std::shared_ptr<mon::HostMetrics> &vhost,
													  const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value empty_body;

			return OnPostPushes(client, empty_body, vhost, app);
		}

		ApiResponse AppActionsController::OnPostPushes(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<RtmpPushPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::RtmpPush));
			if (publisher == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			std::vector<std::shared_ptr<info::Push>> pushes;
			Json::Value response;

			auto error = publisher->GetPushes(app->GetName(), pushes);
			if (error->GetCode() != RtmpPushPublisher::PushPublisherErrorCode::Success || pushes.size() == 0)
			{
				return HttpError::CreateError(HttpStatusCode::NoContent, "There is no pushes information");
			}

			for (auto &item : pushes)
			{
				response.append(conv::JsonFromPush(item));
			}

			return {HttpStatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStartPush(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
														  const std::shared_ptr<mon::HostMetrics> &vhost,
														  const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<RtmpPushPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::RtmpPush));
			if (publisher == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			auto push = conv::PushFromJson(request_body);
			if (push == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}
			push->SetVhost(vhost->GetName().CStr());
			push->SetApplication(app->GetName().GetAppName());

			// logtd("%s", push->GetInfoString().CStr());

			auto error = publisher->PushStart(app->GetName(), push);
			if (error->GetCode() == RtmpPushPublisher::PushPublisherErrorCode::FailureInvalidParameter)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, error->GetMessage());
			}
			else if (error->GetCode() == RtmpPushPublisher::PushPublisherErrorCode::FailureDupulicateKey)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, error->GetMessage());
			}

			Json::Value response;
			response.append(conv::JsonFromPush(push));

			return {HttpStatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStopPush(const std::shared_ptr<HttpClient> &client, const Json::Value &request_body,
														 const std::shared_ptr<mon::HostMetrics> &vhost,
														 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<RtmpPushPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::RtmpPush));
			if (publisher == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			auto push = conv::PushFromJson(request_body);
			if (push == nullptr)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, "Could not parse json context: [%s/%s]",
											  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}
			push->SetVhost(vhost->GetName().CStr());
			push->SetApplication(app->GetName().GetAppName());

			auto error = publisher->PushStop(app->GetName(), push);
			if (error->GetCode() == RtmpPushPublisher::PushPublisherErrorCode::FailureInvalidParameter)
			{
				return HttpError::CreateError(HttpStatusCode::BadRequest, error->GetMessage());
			}
			else if (error->GetCode() == RtmpPushPublisher::PushPublisherErrorCode::FailureNotExist)
			{
				return HttpError::CreateError(HttpStatusCode::NotFound, error->GetMessage());
			}

			return HttpError::CreateError(HttpStatusCode::OK, error->GetMessage());
		}

		ApiResponse AppActionsController::OnGetDummyAction(const std::shared_ptr<HttpClient> &client,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			logte("Called OnGetDummyAction. invoke [%s/%s]",
				  vhost->GetName().CStr(), app->GetName().GetAppName());

			return app->GetConfig().ToJson();
		}

	}  // namespace v1
}  // namespace api
