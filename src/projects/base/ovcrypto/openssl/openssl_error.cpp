//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "openssl_error.h"

#include <openssl/err.h>

namespace ov
{
	OpensslError::OpensslError(int code, const String &message)
		: Error("OpenSSL", code, message)
	{
	}

	OpensslError::OpensslError(const String &message)
		: Error("OpenSSL", message)
	{
	}

	std::shared_ptr<OpensslError> OpensslError::CreateError(const char *format, ...)
	{
		String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<OpensslError>(message);
	}

	std::shared_ptr<OpensslError> OpensslError::CreateErrorFromOpenssl()
	{
		unsigned long error_code = ::ERR_get_error();
		char buffer[1024];

		::ERR_error_string_n(error_code, buffer, OV_COUNTOF(buffer));

		return std::make_shared<OpensslError>(error_code, buffer);
	}

	std::shared_ptr<OpensslError> OpensslError::CreateErrorFromOpenssl(SSL *ssl, int result)
	{
		auto ssl_error = ::SSL_get_error(ssl, result);

		char buffer[1024];

		::ERR_error_string_n(ssl_error, buffer, OV_COUNTOF(buffer));

		return std::make_shared<OpensslError>(ssl_error, buffer);
	}
}  // namespace ov
