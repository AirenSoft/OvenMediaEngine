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

		ApiResponse AppActionsController::OnGetRecords(const std::shared_ptr<HttpClient> &clnt)
		{
			auto &match_result = clnt->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			auto app_name = match_result.GetNamedGroup("app_name");
			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}


			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if(publisher == nullptr)			
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%.*s]", vhost_name.length(), vhost_name.data());	
			}

			info::VHostAppName vhost_app(vhost_name.data(), app_name.data());
			std::vector<std::shared_ptr<info::Record>> record_list;

			auto error = publisher->GetRecords(vhost_app, record_list);
			
			return error;
		}

		ApiResponse AppActionsController::OnPostStartRecord(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body)
		{
			// logte("JsonContext : %s", ov::Json::Stringify(request_body).CStr());

			auto &match_result = clnt->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			auto app_name = match_result.GetNamedGroup("app_name");
			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}



			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if(publisher == nullptr)			
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%.*s]", vhost_name.length(), vhost_name.data());	
			}

			auto record = api::conv::RecordFromJson(request_body);
			if(record == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not parse json context: [%.*s]", vhost_name.length(), vhost_name.data());
			}

			info::VHostAppName vhost_app(vhost_name.data(), app_name.data());

			auto error = publisher->RecordStart(vhost_app, record);

			return error;
		}

		ApiResponse AppActionsController::OnPostStopRecord(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body)
		{
			// logte("JsonContext : %s", ov::Json::Stringify(request_body).CStr());

			auto &match_result = clnt->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			auto app_name = match_result.GetNamedGroup("app_name");
			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}

			auto publisher = std::dynamic_pointer_cast<FilePublisher>(ocst::Orchestrator::GetInstance()->GetPublisherFromType(PublisherType::File));
			if(publisher == nullptr)			
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find publisher: [%.*s]", vhost_name.length(), vhost_name.data());	
			}

			auto record = api::conv::RecordFromJson(request_body);
			if(record == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not parse json context: [%.*s]", vhost_name.length(), vhost_name.data());
			}

			info::VHostAppName vhost_app(vhost_name.data(), app_name.data());

			auto error = publisher->RecordStop(vhost_app, record);

			return error;	
		}


		ApiResponse AppActionsController::OnGetPushes(const std::shared_ptr<HttpClient> &clnt)
		{
			Json::Value response = Json::objectValue;
			return std::move(response);			
		}

		ApiResponse AppActionsController::OnPostStartPush(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body)
		{
			Json::Value response = Json::objectValue;
			return std::move(response);			
		}

		ApiResponse AppActionsController::OnPostStopPush(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body)
		{
			Json::Value response = Json::objectValue;
			return std::move(response);			
		}


		ApiResponse AppActionsController::OnGetDummyAction(const std::shared_ptr<HttpClient> &clnt)
		{
			auto &match_result = clnt->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			auto app_name = match_result.GetNamedGroup("app_name");
			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}

			logte("Called OnGetDummyAction. invoke [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());


			return api::conv::JsonFromApplication(app);
		}

		ApiResponse AppActionsController::OnPostDummyAction(const std::shared_ptr<HttpClient> &clnt, const Json::Value &request_body)
		{
			auto &match_result = clnt->GetRequest()->GetMatchResult();

			auto vhost_name = match_result.GetNamedGroup("vhost_name");
			auto vhost = GetVirtualHost(vhost_name);
			if (vhost == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]",
											  vhost_name.length(), vhost_name.data());
			}

			auto app_name = match_result.GetNamedGroup("app_name");
			auto app = GetApplication(vhost, app_name);
			if (app == nullptr)
			{
				return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find application: [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());
			}

			logte("Called OnPostDummyAction. invoke [%.*s/%.*s]",
											  vhost_name.length(), vhost_name.data(),
											  app_name.length(), app_name.data());


			return api::conv::JsonFromApplication(app);
		}
		
	}  // namespace v1
}  // namespace api
