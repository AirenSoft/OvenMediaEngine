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

#include "http_datastructure.h"

namespace http
{
	class HttpError : public ov::Error
	{
	public:
		HttpError(StatusCode status_code, const ov::String &message)
			: ov::Error(static_cast<int>(status_code), message)
		{
		}

		StatusCode GetStatusCode() const
		{
			return static_cast<StatusCode>(GetCode());
		}

		static std::shared_ptr<const HttpError> CreateError(StatusCode status_code, const char *format, ...)
		{
			ov::String message;
			va_list list;
			va_start(list, format);
			message.VFormat(format, list);
			va_end(list);

			return std::make_shared<const HttpError>(status_code, message);
		}

		static std::shared_ptr<const HttpError> CreateError(const std::shared_ptr<const ov::Error> &error)
		{
			if (error == nullptr)
			{
				return nullptr;
			}

			auto http_error = std::dynamic_pointer_cast<const HttpError>(error);

			if (http_error != nullptr)
			{
				return http_error;
			}

			return HttpError::CreateError(StatusCode::InternalServerError, "%s", error->ToString().CStr());
		}
	};
}  // namespace http
