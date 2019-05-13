//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./string.h"

#include <memory>

enum class HttpStatusCode : uint16_t;

namespace ov
{

	class Error
	{
	public:
		// There is no error
		Error() = default;
		Error(const ov::String &domain, int code);
		Error(const ov::String &domain, const char *format, ...);
		Error(const ov::String &domain, int code, const char *format, ...);

		explicit Error(int code);
		Error(int code, const char *format, ...);

		Error(const Error &error) = default;

		static std::shared_ptr<Error> CreateError(ov::String domain, int code, const char *format, ...);
		static std::shared_ptr<Error> CreateError(ov::String domain, const char *format, ...);
		static std::shared_ptr<Error> CreateError(int code, const char *format, ...);
		static std::shared_ptr<Error> CreateError(HttpStatusCode code, const char *format, ...);
		static std::shared_ptr<Error> CreateErrorFromErrno();
		static std::shared_ptr<Error> CreateErrorFromSrt();
		static std::shared_ptr<Error> CreateErrorFromOpenSsl();

		virtual ~Error() = default;

		virtual int GetCode() const;
		virtual String GetMessage() const;

		virtual String ToString() const;

	private:
		String _domain;

		int _code = 0;
		String _message;
	};
}