//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <memory>

#include "./string.h"

namespace ov
{
	class Error
	{
	public:
		Error(const Error &error) = default;

		// There is no error
		Error() = default;
		Error(ov::String domain, int code);
		Error(ov::String domain, ov::String message);
		Error(ov::String domain, int code, ov::String message);

		explicit Error(int code);
		Error(int code, ov::String message);

		static std::shared_ptr<Error> CreateError(ov::String domain, int code, const char *format, ...);
		static std::shared_ptr<Error> CreateError(ov::String domain, const char *format, ...);
		static std::shared_ptr<Error> CreateError(int code, const char *format, ...);
		static std::shared_ptr<Error> CreateErrorFromErrno();
		static std::shared_ptr<Error> CreateErrorFromSrt();
		static std::shared_ptr<Error> CreateErrorFromOpenSsl();

		virtual ~Error() = default;

		virtual int GetCode() const;
		virtual String GetMessage() const;

		virtual String ToString() const;

	protected:
		String _domain;

		int _code = 0;
		bool _code_set = false;

		String _message;
	};
}  // namespace ov
