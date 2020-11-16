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
		int code = static_cast<int>(status_code);

		if ((code / 100) != 2)
		{
			_json["code"] = code;
			_json["message"] = StringFromHttpStatusCode(status_code);
		}
		else
		{
			_json = Json::objectValue;
		}
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
		if (error == nullptr)
		{
			_json = Json::objectValue;
			return;
		}

		_status_code = static_cast<HttpStatusCode>(error->GetCode());

		if (IsValidHttpStatusCode(_status_code) == false)
		{
			_status_code = HttpStatusCode::InternalServerError;
		}

		_json["code"] = error->GetCode();
		_json["message"] = error->ToString().CStr();
	}

	ApiResponse::ApiResponse(const ApiResponse &response)
	{
		_status_code = response._status_code;
		_json = response._json;
	}

	ApiResponse::ApiResponse(ApiResponse &&response)
	{
		_status_code = std::move(response._status_code);
		_json = std::move(response._json);
	}

	bool ApiResponse::SendToClient(const std::shared_ptr<HttpClient> &client)
	{
		const auto &response = client->GetResponse();

		response->SetStatusCode(_status_code);
		response->SetHeader("Content-Type", "application/json;charset=UTF-8");

		return (_json.isNull() == false) ? response->AppendString(ov::Json::Stringify(_json)) : true;
	}
}  // namespace api