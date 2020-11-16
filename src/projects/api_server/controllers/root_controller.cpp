//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "root_controller.h"

#include "v1/v1_controller.h"

namespace api
{
	void RootController::PrepareHandlers()
	{
		// Currently only v1 is supported
		CreateSubController<v1::V1Controller>(R"(\/v1)");

		// This handler is called if it does not match all other registered handlers
		Register(HttpMethod::All, R"(.+)", &RootController::OnNotFound);
	};

	ApiResponse RootController::OnNotFound(const std::shared_ptr<HttpClient> &client)
	{
		return ov::Error::CreateError(HttpStatusCode::NotFound, "Controller not found");
	}
}  // namespace api