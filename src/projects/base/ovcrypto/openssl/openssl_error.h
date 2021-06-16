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
		OpensslError(int code, const String &message);
		OpensslError(const String &message);

		static std::shared_ptr<OpensslError> CreateError(const char *format, ...);
		static std::shared_ptr<OpensslError> CreateErrorFromOpenssl();
		static std::shared_ptr<OpensslError> CreateErrorFromOpenssl(SSL *ssl, int result);
	};
}  // namespace ov