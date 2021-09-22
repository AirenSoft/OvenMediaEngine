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

#include <memory>

#define CreateConfigError(format, ...) \
	cfg::ConfigError::CreateError(__FILE__, __LINE__, format, ##__VA_ARGS__)

// Just a hint of what exception is thrown
#define MAY_THROWS(...)

namespace cfg
{
	class ConfigError : public ov::Error
	{
	public:
		ConfigError(const char *file_name, int line_number, ov::String message);

		static std::shared_ptr<ConfigError> CreateError(const char *file_name, int line_number, const char *format, ...);

		ov::String GetDetailedMessage() const;

	protected:
		ov::String _file_name;
		int _line_number;
	};
}  // namespace cfg
