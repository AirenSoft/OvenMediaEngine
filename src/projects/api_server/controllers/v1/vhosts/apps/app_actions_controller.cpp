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

			//----------------------------------------
			// Record related actions
			//----------------------------------------
			RegisterPost(R"((records))", &AppActionsController::OnPostRecords);
			RegisterPost(R"((startRecord))", &AppActionsController::OnPostStartRecord);
			RegisterPost(R"((stopRecord))", &AppActionsController::OnPostStopRecord);
			// @GET action will be deprecated
			RegisterGet(R"((records))", &AppActionsController::OnGetRecords);

			//----------------------------------------
			// Push related actions
			//----------------------------------------
			RegisterPost(R"((pushes))", &AppActionsController::OnPostPushes);
			RegisterPost(R"((startPush))", &AppActionsController::OnPostStartPush);
			RegisterPost(R"((stopPush))", &AppActionsController::OnPostStopPush);
			// @GET action will be deprecated
			RegisterGet(R"((pushes))", &AppActionsController::OnGetPushes);
		};

		ApiResponse AppActionsController::OnGetRecords(const std::shared_ptr<http::svr::HttpConnection> &client,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value empty_body;

			return OnPostRecords(client, empty_body, vhost, app);
		}

		ApiResponse AppActionsController::OnPostRecords(const std::shared_ptr<http::svr::HttpConnection> &client,
														const Json::Value &request_body,
														const std::shared_ptr<mon::HostMetrics> &vhost,
														const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response;

			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::NotFound,
													"Could not find publisher: [%s/%s]",
													vhost->GetName().CStr(),
													app->GetName().GetAppName().CStr());
			}

			std::vector<std::shared_ptr<info::Record>> records;

			auto error = publisher->GetRecords(app->GetName(), records);
			if (error->GetCode() != FilePublisher::FilePublisherStatusCode::Success || records.size() == 0)
			{
				return http::HttpError::CreateError(http::StatusCode::NotFound,
													"There is no record information");
			}

			for (auto &item : records)
			{
				response.append(::serdes::JsonFromRecord(item));
			}

			return {http::StatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStartRecord(const std::shared_ptr<http::svr::HttpConnection> &client,
															const Json::Value &request_body,
															const std::shared_ptr<mon::HostMetrics> &vhost,
															const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response;

			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::NotFound,
													"Could not find publisher: [%s/%s]",
													vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			auto record = ::serdes::RecordFromJson(request_body);
			if (record == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::BadRequest,
													"Could not parse json context: [%s/%s]",
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
				return http::HttpError::CreateError(http::StatusCode::BadRequest,
													error->GetMessage());
			}
			else if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureDupulicateKey)
			{
				return http::HttpError::CreateError(http::StatusCode::BadRequest,
													error->GetMessage());
			}

			response.append(::serdes::JsonFromRecord(record));

			return {http::StatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStopRecord(const std::shared_ptr<http::svr::HttpConnection> &client,
														   const Json::Value &request_body,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response;

			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::NotFound,
													"Could not find publisher: [%s/%s]",
													vhost->GetName().CStr(),
													app->GetName().GetAppName().CStr());
			}

			auto record = ::serdes::RecordFromJson(request_body);
			if (record == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::BadRequest,
													"Could not parse json context: [%s/%s]",
													vhost->GetName().CStr(),
													app->GetName().GetAppName().CStr());
			}
			else
			{
				record->SetVhost(vhost->GetName().CStr());
				record->SetApplication(app->GetName().GetAppName());
			}

			auto error = publisher->RecordStop(app->GetName(), record);
			if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureInvalidParameter)
			{
				return http::HttpError::CreateError(http::StatusCode::BadRequest, error->GetMessage());
			}
			else if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureNotExist)
			{
				return http::HttpError::CreateError(http::StatusCode::NotFound, error->GetMessage());
			}

			response.append(::serdes::JsonFromRecord(record));

			return {http::StatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnGetPushes(const std::shared_ptr<http::svr::HttpConnection> &client,
													  const std::shared_ptr<mon::HostMetrics> &vhost,
													  const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value empty_body;

			return OnPostPushes(client, empty_body, vhost, app);
		}

		ApiResponse AppActionsController::OnPostPushes(const std::shared_ptr<http::svr::HttpConnection> &client,
													   const Json::Value &request_body,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			std::vector<std::shared_ptr<info::Push>> pushes;
			Json::Value response;

			auto publisher = std::dynamic_pointer_cast<RtmpPushPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::RtmpPush));
			if (publisher == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::NotFound,
													"Could not find publisher: [%s/%s]",
													vhost->GetName().CStr(),
													app->GetName().GetAppName().CStr());
			}

			auto error = publisher->GetPushes(app->GetName(), pushes);
			if (error->GetCode() != RtmpPushPublisher::PushPublisherErrorCode::Success || pushes.size() == 0)
			{
				return http::HttpError::CreateError(http::StatusCode::NoContent, "There is no pushes information");
			}

			for (auto &item : pushes)
			{
				response.append(::serdes::JsonFromPush(item));
			}

			return {http::StatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStartPush(const std::shared_ptr<http::svr::HttpConnection> &client,
														  const Json::Value &request_body,
														  const std::shared_ptr<mon::HostMetrics> &vhost,
														  const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response;

			auto publisher = std::dynamic_pointer_cast<RtmpPushPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::RtmpPush));
			if (publisher == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::NotFound,
													"Could not find publisher: [%s/%s]",
													vhost->GetName().CStr(),
													app->GetName().GetAppName().CStr());
			}

			auto push = ::serdes::PushFromJson(request_body);
			if (push == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::BadRequest,
													"Could not parse json context: [%s/%s]",
													vhost->GetName().CStr(),
													app->GetName().GetAppName().CStr());
			}

			push->SetVhost(vhost->GetName().CStr());
			push->SetApplication(app->GetName().GetAppName());

			auto error = publisher->PushStart(app->GetName(), push);
			switch (error->GetCode())
			{
				case RtmpPushPublisher::PushPublisherErrorCode::FailureInvalidParameter: {
					return http::HttpError::CreateError(http::StatusCode::BadRequest, error->GetMessage());
				}
				break;
				case RtmpPushPublisher::PushPublisherErrorCode::FailureDupulicateKey: {
					return http::HttpError::CreateError(http::StatusCode::BadRequest, error->GetMessage());
				}
				break;
			}

			auto url = ov::Url::Parse(client->GetRequest()->GetUri());
			auto a_name = app->GetName();
			auto s_name = push->GetStreamName();
			ocst::Orchestrator::GetInstance()->RequestPullStream(url, a_name, s_name);

			response.append(::serdes::JsonFromPush(push));

			return {http::StatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStopPush(const std::shared_ptr<http::svr::HttpConnection> &client,
														 const Json::Value &request_body,
														 const std::shared_ptr<mon::HostMetrics> &vhost,
														 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto publisher = std::dynamic_pointer_cast<RtmpPushPublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::RtmpPush));
			if (publisher == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::NotFound,
													"Could not find publisher: [%s/%s]",
													vhost->GetName().CStr(),
													app->GetName().GetAppName().CStr());
			}

			auto push = ::serdes::PushFromJson(request_body);
			if (push == nullptr)
			{
				return http::HttpError::CreateError(http::StatusCode::BadRequest,
													"Could not parse json context: [%s/%s]",
													vhost->GetName().CStr(),
													app->GetName().GetAppName().CStr());
			}

			push->SetVhost(vhost->GetName().CStr());
			push->SetApplication(app->GetName().GetAppName());

			auto error = publisher->PushStop(app->GetName(), push);
			switch (error->GetCode())
			{
				case RtmpPushPublisher::PushPublisherErrorCode::FailureInvalidParameter: {
					return http::HttpError::CreateError(http::StatusCode::BadRequest, error->GetMessage());
				}
				break;
				case RtmpPushPublisher::PushPublisherErrorCode::FailureNotExist: {
					return http::HttpError::CreateError(http::StatusCode::NotFound, error->GetMessage());
				}
				break;
			}

			return http::HttpError::CreateError(http::StatusCode::OK, error->GetMessage());
		}

		ApiResponse AppActionsController::OnGetDummyAction(const std::shared_ptr<http::svr::HttpConnection> &client,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			logte("Called OnGetDummyAction. invoke [%s/%s]", vhost->GetName().CStr(), app->GetName().GetAppName());

			return app->GetConfig().ToJson();
		}

	}  // namespace v1
}  // namespace api
