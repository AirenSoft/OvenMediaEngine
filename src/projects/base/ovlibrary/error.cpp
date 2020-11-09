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
#include <openssl/err.h>
#include <srt/srt.h>

#include "log.h"
#include "memory_utilities.h"

namespace ov
{
	Error::Error(const ov::String &domain, int code)
		: _domain(domain),

		  _code(code),
		  _code_set(true)
	{
	}

	Error::Error(const ov::String &domain, const char *format, ...)
		: _domain(domain)
	{
		va_list list;
		va_start(list, format);
		_message.VFormat(format, list);
		va_end(list);
	}

	Error::Error(const ov::String &domain, int code, const char *format, ...)
		: _domain(domain),

		  _code(code),
		  _code_set(true)
	{
		va_list list;
		va_start(list, format);
		_message.VFormat(format, list);
		va_end(list);
	}

	Error::Error(int code)
		: _code(code),
		  _code_set(true)
	{
	}

	Error::Error(int code, const char *format, ...)
		: _code(code),
		  _code_set(true)
	{
		va_list list;
		va_start(list, format);
		_message.VFormat(format, list);
		va_end(list);
	}

	std::shared_ptr<Error> Error::CreateError(ov::String domain, int code, const char *format, ...)
	{
		String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<Error>(domain, code, message);
	}

	std::shared_ptr<Error> Error::CreateError(ov::String domain, const char *format, ...)
	{
		String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<Error>(domain, message);
	}

	std::shared_ptr<Error> Error::CreateError(int code, const char *format, ...)
	{
		String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<Error>(code, message);
	}

	std::shared_ptr<Error> Error::CreateError(HttpStatusCode code, const char *format, ...)
	{
		String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<Error>(static_cast<int>(code), message);
	}

	std::shared_ptr<Error> Error::CreateErrorFromErrno()
	{
		return std::make_shared<Error>("errno", errno, "%s", strerror(errno));
	}

	std::shared_ptr<Error> Error::CreateErrorFromSrt()
	{
		return std::make_shared<Error>("SRT", srt_getlasterror(nullptr), "%s (0x%x)", srt_getlasterror_str(), srt_getlasterror(nullptr));
	}

	std::shared_ptr<Error> Error::CreateErrorFromOpenSsl()
	{
		unsigned long error_code = ERR_get_error();
		char buffer[1024];

		ERR_error_string_n(error_code, buffer, OV_COUNTOF(buffer));

		return std::make_shared<Error>("OpenSSL", error_code, "%s (0x%x)", buffer, error_code);
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