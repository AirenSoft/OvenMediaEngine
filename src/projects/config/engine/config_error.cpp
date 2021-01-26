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
	ConfigError::ConfigError(ov::String message)
		: ov::Error("Config", message)
	{
	}

	std::shared_ptr<ConfigError> ConfigError::CreateError(const char *format, ...)
	{
		ov::String message;
		va_list list;
		va_start(list, format);
		message.VFormat(format, list);
		va_end(list);

		return std::make_shared<ConfigError>(message);
	}
}  // namespace cfg
