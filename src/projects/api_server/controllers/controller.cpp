//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./controller.h"

namespace api
{
	ApiResponse::ApiResponse(HttpStatusCode status_code)
	{
		_status_code = status_code;
	}

	ApiResponse::ApiResponse(HttpStatusCode status_code, const Json::Value &json)
	{
		_status_code = status_code;
		_json = json;
	}

	ApiResponse::ApiResponse(const Json::Value &json)
	{
		_json = json;
	}

	ApiResponse::ApiResponse(const std::shared_ptr<ov::Error> &error)
	{
		_status_code = static_cast<HttpStatusCode>(error->GetCode());

		if (IsValidHttpStatusCode(_status_code) == false)
		{
			_status_code = HttpStatusCode::InternalServerError;
		}

		_json["message"] = error->ToString().CStr();
	}

	bool ApiResponse::SendToClient(const std::shared_ptr<HttpClient> &client)
	{
		const auto &response = client->GetResponse();

		response->SetStatusCode(_status_code);

		return (_json.isNull() == false) ? response->AppendString(ov::Json::Stringify(_json)) : true;
	}
}  // namespace api