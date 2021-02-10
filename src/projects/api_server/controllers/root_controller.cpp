//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "root_controller.h"

#include <base/ovcrypto/ovcrypto.h>

#include "v1/v1_controller.h"

namespace api
{
	RootController::RootController(const ov::String &access_token)
		: _access_token(access_token)
	{
	}

	void RootController::PrepareHandlers()
	{
		// Prepare a handler to verify that the token sent with the "authorized" header matches the configuration of Server.xml:
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
			auto authorization = client->GetRequest()->GetHeader("Authorization");

			ov::String message = nullptr;

			if (authorization.GetLength() > 0)
			{
				auto tokens = authorization.Split(" ");

				if (tokens.size() == 2)
				{
					if (tokens[0].UpperCaseString() == "BASIC")
					{
						auto data = ov::Base64::Decode(tokens[1]);

						if (data != nullptr)
						{
							ov::String str = data->ToString();

							if (str == _access_token)
							{
								return HttpNextHandler::Call;
							}
							else
							{
								message = "Invalid credential";
							}
						}
						else
						{
							message = "Invalid credential format";
						}
					}
					else
					{
						message.AppendFormat("Not supported credential type: %s", tokens[0].CStr());
					}
				}
				else
				{
					// Invalid tokens
					message = "Invalid authorization header";
				}
			}
			else
			{
				message = "Authorization header is required to call API";
			}

			ApiResponse response(HttpError::CreateError(HttpStatusCode::Forbidden, message));
			response.SendToClient(client);

			return HttpNextHandler::DoNotCall;
		});
	}

	ApiResponse RootController::OnNotFound(const std::shared_ptr<HttpClient> &client)
	{
		return HttpError::CreateError(HttpStatusCode::NotFound, "Controller not found");
	}
}  // namespace api