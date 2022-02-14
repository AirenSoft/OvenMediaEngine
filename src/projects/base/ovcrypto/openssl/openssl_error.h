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
#include <openssl/ssl.h>

namespace ov
{
	class OpensslError : public Error
	{
	public:
		// Create an error instance from OpenSSL error code
		OpensslError();
		OpensslError(ov::String message);
		OpensslError(const char *format, ...);
		OpensslError(SSL *ssl, int result);
	};
}  // namespace ov