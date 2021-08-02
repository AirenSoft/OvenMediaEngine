//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "others.h"

namespace serdes
{
	Json::Value JsonFromError(const std::shared_ptr<http::HttpError> &error)
	{
		Json::Value value(Json::ValueType::nullValue);

		if (error != nullptr)
		{
			value["statusCode"] = error->GetCode();
			value["message"] = error->GetMessage().CStr();
		}

		return value;
	}
}  // namespace serdes