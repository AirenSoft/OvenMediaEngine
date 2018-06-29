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

namespace ov
{
	class Error
	{
	public:
		// 에러가 없는 상태
		Error();
		Error(const ov::String &domain, int code);
		Error(const ov::String &domain, int code, const char *format, ...);

		Error(int code);
		Error(int code, const char *format, ...);

		Error(const Error &error);

		static std::shared_ptr<Error> CreateError(ov::String domain, int code, const char *format, ...);
		static std::shared_ptr<Error> CreateError(int code, const char *format, ...);
		static std::shared_ptr<Error> CreateErrorFromErrno();

		virtual ~Error();

		virtual int GetCode() const;
		virtual String GetMessage() const;

		virtual String ToString() const;

	protected:
		void Initialize(ov::String domain, int code, String message);

	private:
		String _domain;

		int _code;
		String _message;
	};
}