//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_error.h"

namespace cfg
{
	ConfigError::ConfigError(const char *file_name, int line_number, ov::String message)
		: ov::Error("Config", message),

		  _file_name(file_name),
		  _line_number(line_number)
	{
	}

	std::shared_ptr<ConfigError> ConfigError::CreateError(const char *file_name, int line_number, const char *format, ...)
	{
		ov::String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<ConfigError>(file_name, line_number, message);
	}

	ov::String ConfigError::GetDetailedMessage() const
	{
		auto message = GetMessage();

		message.AppendFormat(" (%s:%d)", _file_name.CStr(), _line_number);

		return message;
	}
}  // namespace cfg
