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

#define HTTP_ERROR_DOMAIN "HTTP"

namespace http
{
	class HttpError : public ov::Error
	{
	public:
		HttpError(StatusCode status_code, const ov::String &message)
			: ov::Error(HTTP_ERROR_DOMAIN, static_cast<int>(status_code), message)
		{
		}

		HttpError(StatusCode status_code, const char *format, ...)
			: ov::Error(HTTP_ERROR_DOMAIN)
		{
			ov::String message;
			va_list list;
			va_start(list, format);
			message.VFormat(format, list);
			va_end(list);

			SetCodeAndMessage(static_cast<int>(status_code), message);
		}

		StatusCode GetStatusCode() const
		{
			return static_cast<StatusCode>(GetCode());
		}
	};
}  // namespace http
