//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

class HttpError : public ov::Error
{
public:
	HttpError(HttpStatusCode status_code, const ov::String &message)
		: ov::Error(static_cast<int>(status_code), message)
	{
	}

	HttpStatusCode GetStatusCode() const
	{
		return static_cast<HttpStatusCode>(GetCode());
	}

	static std::shared_ptr<HttpError> CreateError(HttpStatusCode status_code, const char *format, ...)
	{
		ov::String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<HttpError>(status_code, message);
	}

	static std::shared_ptr<HttpError> CreateError(const std::shared_ptr<ov::Error> &error)
	{
		if (error == nullptr)
		{
			return nullptr;
		}

		auto http_error = std::dynamic_pointer_cast<HttpError>(error);

		if (http_error != nullptr)
		{
			return http_error;
		}

		return HttpError::CreateError(HttpStatusCode::InternalServerError, "%s", error->ToString().CStr());
	}
};