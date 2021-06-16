//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "error.h"

#include <errno.h>

#include "log.h"
#include "memory_utilities.h"

namespace ov
{
	Error::Error(ov::String domain, int code)
		: _domain(std::move(domain)),

		  _code(code),
		  _code_set(true)
	{
	}

	Error::Error(ov::String domain, ov::String message)
		: _domain(std::move(domain)),
		  _message(std::move(message))
	{
	}

	Error::Error(ov::String domain, int code, ov::String message)
		: _domain(std::move(domain)),

		  _code(code),
		  _code_set(true),

		  _message(std::move(message))
	{
	}

	Error::Error(int code)
		: _code(code),
		  _code_set(true)
	{
	}

	Error::Error(int code, ov::String message)
		: _code(code),
		  _code_set(true),

		  _message(std::move(message))
	{
	}

	std::shared_ptr<Error> Error::CreateError(ov::String domain, int code, const char *format, ...)
	{
		String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<Error>(domain, code, std::move(message));
	}

	std::shared_ptr<Error> Error::CreateError(ov::String domain, const char *format, ...)
	{
		String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<Error>(domain, std::move(message));
	}

	std::shared_ptr<Error> Error::CreateError(int code, const char *format, ...)
	{
		String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<Error>(code, std::move(message));
	}

	std::shared_ptr<Error> Error::CreateErrorFromErrno()
	{
		return ov::Error::CreateError("errno", errno, "%s", ::strerror(errno));
	}

	int Error::GetCode() const
	{
		return _code;
	}

	String Error::GetMessage() const
	{
		return _message;
	}

	String Error::ToString() const
	{
		String description;

		if (_domain.IsEmpty() == false)
		{
			description.AppendFormat("[%s] ", _domain.CStr());
		}

		if (_message.IsEmpty() == false)
		{
			description.AppendFormat("%s", _message.CStr());
		}
		else
		{
			description.AppendFormat("(No error message)");
		}

		if (_code_set)
		{
			description.AppendFormat(" (%d)", _code);
		}

		return description;
	}
}  // namespace ov
