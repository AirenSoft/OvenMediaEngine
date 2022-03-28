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

		ApiResponse AppActionsController::OnGetRecords(const std::shared_ptr<http::svr::HttpExchange> &client,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value empty_body;

			return OnPostRecords(client, empty_body, vhost, app);
		}

		ApiResponse AppActionsController::OnPostRecords(const std::shared_ptr<http::svr::HttpExchange> &client,
														const Json::Value &request_body,
														const std::shared_ptr<mon::HostMetrics> &vhost,
														const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response;

			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find publisher: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			auto application = std::static_pointer_cast<FileApplication>(publisher->GetApplicationByName(app->GetName()));
			if (application == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find application: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			auto record = ::serdes::RecordFromJson(request_body);
			if (record == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}
			else
			{
				record->SetVhost(vhost->GetName().CStr());
				record->SetApplication(app->GetName().GetAppName());
			}

			std::vector<std::shared_ptr<info::Record>> results;

			auto error = application->GetRecords(record, results);
			if (error->GetCode() != FilePublisher::FilePublisherStatusCode::Success || results.size() == 0)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "There is no record information");
			}

			for (auto &item : results)
			{
				response.append(::serdes::JsonFromRecord(item));
			}

			return response;
		}

		ApiResponse AppActionsController::OnPostStartRecord(const std::shared_ptr<http::svr::HttpExchange> &client,
															const Json::Value &request_body,
															const std::shared_ptr<mon::HostMetrics> &vhost,
															const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response;

			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find publisher: [%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}

			auto application = std::static_pointer_cast<FileApplication>(publisher->GetApplicationByName(app->GetName()));
			if (application == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find application: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			auto record = ::serdes::RecordFromJson(request_body);
			if (record == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s]",
									  vhost->GetName().CStr(), app->GetName().GetAppName().CStr());
			}
			else
			{
				record->SetVhost(vhost->GetName().CStr());
				record->SetApplication(app->GetName().GetAppName());
			}

			auto error = application->RecordStart(record);
			if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureInvalidParameter)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  error->GetMessage());
			}
			else if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureDupulicateKey)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  error->GetMessage());
			}

			response.append(::serdes::JsonFromRecord(record));

			return {http::StatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStopRecord(const std::shared_ptr<http::svr::HttpExchange> &client,
														   const Json::Value &request_body,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response;

			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if (publisher == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find publisher: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			auto application = std::static_pointer_cast<FileApplication>(publisher->GetApplicationByName(app->GetName()));
			if (application == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find application: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			auto record = ::serdes::RecordFromJson(request_body);
			if (record == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}
			else
			{
				record->SetVhost(vhost->GetName().CStr());
				record->SetApplication(app->GetName().GetAppName());
			}

			auto error = application->RecordStop(record);
			if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureInvalidParameter)
			{
				throw http::HttpError(http::StatusCode::BadRequest, error->GetMessage());
			}
			else if (error->GetCode() == FilePublisher::FilePublisherStatusCode::FailureNotExist)
			{
				throw http::HttpError(http::StatusCode::NotFound, error->GetMessage());
			}

			response.append(::serdes::JsonFromRecord(record));

			return {http::StatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnGetPushes(const std::shared_ptr<http::svr::HttpExchange> &client,
													  const std::shared_ptr<mon::HostMetrics> &vhost,
													  const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value empty_body;

			return OnPostPushes(client, empty_body, vhost, app);
		}

		ApiResponse AppActionsController::OnPostPushes(const std::shared_ptr<http::svr::HttpExchange> &client,
													   const Json::Value &request_body,
													   const std::shared_ptr<mon::HostMetrics> &vhost,
													   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			std::vector<std::shared_ptr<info::Push>> results;
			Json::Value response;

			auto push{::serdes::PushFromJson(request_body)};
			if (push == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			std::vector<PublisherType> publisher_types{PublisherType::RtmpPush, PublisherType::MpegtsPush};
			for (auto publisher_type : publisher_types)
			{
				auto publisher{
					ocst::Orchestrator::GetInstance()->GetPublisherFromType(publisher_type)};
				if (publisher == nullptr)
				{
					throw http::HttpError(http::StatusCode::NotFound,
										  "Could not find publisher: [%s/%s]",
										  vhost->GetName().CStr(),
										  app->GetName().GetAppName().CStr());
				}

				auto application{
					std::static_pointer_cast<pub::PushApplication>(publisher->GetApplicationByName(app->GetName()))};
				if (application != nullptr)
				{
					application->GetPushes(push, results);
				}
			}

			if (results.size() == 0)
			{
				throw http::HttpError(http::StatusCode::NoContent, "There is no pushes information");
			}

			for (auto &item : results)
			{
				response.append(::serdes::JsonFromPush(item));
			}

			return {http::StatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStartPush(const std::shared_ptr<http::svr::HttpExchange> &client,
														  const Json::Value &request_body,
														  const std::shared_ptr<mon::HostMetrics> &vhost,
														  const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			Json::Value response;

			auto push{::serdes::PushFromJson(request_body)};
			if (push == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			auto push_protocol{push->GetProtocol().LowerCaseString()};
			auto publisher_type{
				(push_protocol == "rtmp") ? PublisherType::RtmpPush : (push_protocol == "mpegts") ? PublisherType::MpegtsPush
																								  : PublisherType::Unknown};

			if (publisher_type == PublisherType::Unknown)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not find protocol '%s': [%s/%s]",
									  push_protocol.CStr(),
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			auto publisher{
				ocst::Orchestrator::GetInstance()->GetPublisherFromType(publisher_type)};
			if (publisher == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find publisher: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			auto application{
				std::static_pointer_cast<pub::PushApplication>(publisher->GetApplicationByName(app->GetName()))};
			if (application == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find application: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			push->SetVhost(vhost->GetName().CStr());
			push->SetApplication(app->GetName().GetAppName());

			auto error{application->PushStart(push)};
			if (error->GetCode() != pub::PushApplication::ErrorCode::Success)
			{
				throw http::HttpError(http::StatusCode::BadRequest, error->GetMessage());
			}

			auto url = ov::Url::Parse(client->GetRequest()->GetUri());
			auto a_name = app->GetName();
			auto s_name = push->GetStreamName();
			ocst::Orchestrator::GetInstance()->RequestPullStream(url, a_name, s_name);

			response.append(::serdes::JsonFromPush(push));

			return {http::StatusCode::OK, std::move(response)};
		}

		ApiResponse AppActionsController::OnPostStopPush(const std::shared_ptr<http::svr::HttpExchange> &client,
														 const Json::Value &request_body,
														 const std::shared_ptr<mon::HostMetrics> &vhost,
														 const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			auto push{::serdes::PushFromJson(request_body)};
			if (push == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s]",
									  vhost->GetName().CStr(),
									  app->GetName().GetAppName().CStr());
			}

			push->SetVhost(vhost->GetName().CStr());
			push->SetApplication(app->GetName().GetAppName());

			std::vector<PublisherType> publisher_types{PublisherType::RtmpPush, PublisherType::MpegtsPush};
			for (auto publisher_type : publisher_types)
			{
				auto publisher{
					ocst::Orchestrator::GetInstance()->GetPublisherFromType(publisher_type)};
				if (publisher == nullptr)
				{
					throw http::HttpError(http::StatusCode::NotFound,
										  "Could not find publisher: [%s/%s]",
										  vhost->GetName().CStr(),
										  app->GetName().GetAppName().CStr());
				}

				auto application{
					std::static_pointer_cast<pub::PushApplication>(publisher->GetApplicationByName(app->GetName()))};
				if (application != nullptr)
				{
					auto error{application->PushStop(push)};
					switch (error->GetCode())
					{
						case pub::PushApplication::ErrorCode::FailureInvalidParameter: {
							throw http::HttpError(http::StatusCode::BadRequest, error->GetMessage());
						}
						case pub::PushApplication::ErrorCode::Success: {
							throw http::HttpError(http::StatusCode::OK, error->GetMessage());
						}
					}
				}
			}

			throw http::HttpError(http::StatusCode::NotFound,
								  "Could not find id '%s': [%s/%s]",
								  push->GetId().CStr(),
								  vhost->GetName().CStr(),
								  app->GetName().GetAppName().CStr());
		}

		ApiResponse AppActionsController::OnGetDummyAction(const std::shared_ptr<http::svr::HttpExchange> &client,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app)
		{
			logte("Called OnGetDummyAction. invoke [%s/%s]", vhost->GetName().CStr(), app->GetName().GetAppName());

			return app->GetConfig().ToJson();
		}

	}  // namespace v1
}  // namespace api
