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

#if DEBUG
#	define CreateConfigError(format, ...) \
		cfg::ConfigError::CreateError(format " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)
#else  // DEBUG
#	define CreateConfigError(format, ...) \
		cfg::ConfigError::CreateError(format, ##__VA_ARGS__)
#endif	// DEBUG

// Just a hint of what exception is thrown
#define MAY_THROWS(...)

namespace cfg
{
	class ConfigError : public ov::Error
	{
	public:
		ConfigError(ov::String message);

		static std::shared_ptr<ConfigError> CreateError(const char *format, ...);
	};
}  // namespace cfg
