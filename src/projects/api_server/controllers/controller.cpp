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
		SetResponse(status_code);
	}

	ApiResponse::ApiResponse(HttpStatusCode status_code, const Json::Value &json)
	{
		SetResponse(status_code, nullptr, json);
	}

	ApiResponse::ApiResponse(MultipleStatus status_code, const Json::Value &json)
	{
		if (json.isArray())
		{
			_status_code = status_code.GetStatusCode();
			_json = json;
		}
		else
		{
			SetResponse(status_code.GetStatusCode(), nullptr, json);
		}
	}

	ApiResponse::ApiResponse(const Json::Value &json)
	{
		SetResponse(HttpStatusCode::OK, nullptr, json);
	}

	ApiResponse::ApiResponse(const std::shared_ptr<HttpError> &error)
	{
		if (error == nullptr)
		{
			SetResponse(HttpStatusCode::OK);
			return;
		}

		SetResponse(error->GetStatusCode(), error->ToString().CStr());
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

	void ApiResponse::SetResponse(HttpStatusCode status_code)
	{
		SetResponse(status_code, nullptr);
	}

	void ApiResponse::SetResponse(HttpStatusCode status_code, const char *message)
	{
		_status_code = status_code;

		_json["statusCode"] = static_cast<int>(status_code);
		_json["message"] = (message == nullptr) ? StringFromHttpStatusCode(status_code) : message;
	}

	void ApiResponse::SetResponse(HttpStatusCode status_code, const char *message, const Json::Value &json)
	{
		SetResponse(status_code, message);

		_json["response"] = json;
	}

	bool ApiResponse::SendToClient(const std::shared_ptr<HttpClient> &client)
	{
		const auto &response = client->GetResponse();

		response->SetStatusCode(_status_code);
		response->SetHeader("Content-Type", "application/json;charset=UTF-8");

		return (_json.isNull() == false) ? response->AppendString(ov::Json::Stringify(_json)) : true;
	}
}  // namespace api