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
	RootController::RootController(const ov::String &access_token)
		: _access_token(access_token)
	{
	}

	void RootController::PrepareHandlers()
	{
		// Prepare a handler to verify that the token sent with the "access_token" query string matches the configuration of Server.xml:
		//
		// <Server>
		//     <Managers>
		//         <API>
		//             <AccessToken>ometest</AccessToken>
		//         </API>
		//     </Managers>
		// </Server>
		//
		// This handler must be installed before any other handler.
		PrepareAccessTokenHandler();

		// Currently only v1 is supported
		CreateSubController<v1::V1Controller>(R"(\/v1)");

		// This handler is called if it does not match all other registered handlers
		Register(HttpMethod::All, R"(.+)", &RootController::OnNotFound);
	};

	void RootController::PrepareAccessTokenHandler()
	{
		_interceptor->Register(HttpMethod::All, R"(.+)", [=](const std::shared_ptr<HttpClient> &client) -> HttpNextHandler {
			auto url = ov::Url::Parse(client->GetRequest()->GetUri());

			auto access_token = url->GetQueryValue("access_token");

			if (access_token == _access_token)
			{
				return HttpNextHandler::Call;
			}

			ApiResponse response(
				HttpError::CreateError(
					HttpStatusCode::Forbidden, "Invalid access token"));

			response.SendToClient(client);

			return HttpNextHandler::DoNotCall;
		});
	}

	ApiResponse RootController::OnNotFound(const std::shared_ptr<HttpClient> &client)
	{
		return HttpError::CreateError(HttpStatusCode::NotFound, "Controller not found");
	}
}  // namespace api