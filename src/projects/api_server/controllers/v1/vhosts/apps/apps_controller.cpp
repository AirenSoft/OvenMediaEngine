//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "apps_controller.h"

#include <functional>

#include "../../../../api_private.h"

namespace api
{
	namespace v1
	{
		void AppsController::PrepareHandlers()
		{
			RegisterGet(R"()", &AppsController::OnGetAppList);
			RegisterGet(R"(\/(?<app>[^\/]*))", &AppsController::OnGetApp);
		};

		ApiResponse AppsController::OnGetAppList(const std::shared_ptr<HttpClient> &client)
		{
			return {HttpStatusCode::InternalServerError};
		}

		ApiResponse AppsController::OnGetApp(const std::shared_ptr<HttpClient> &client)
		{
			return {HttpStatusCode::InternalServerError};
		}
	}  // namespace v1
}  // namespace api
