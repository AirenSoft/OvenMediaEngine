//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pvd
			{
				struct Provider : public Item
				{
				protected:
					int _max_connection = 0;
					TimestampMode _timestamp_mode = TimestampMode::Auto;
					bool _use_incoming_timestamp = false; // For backward compatibility
					ov::String _timestamp_mode_str;

				public:
					virtual ProviderType GetType() const = 0;
					CFG_DECLARE_CONST_REF_GETTER_OF(GetMaxConnection, _max_connection)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetTimestampMode, _timestamp_mode)

				protected:
					void MakeList() override
					{
						Register<Optional>("MaxConnection", &_max_connection);
						Register<Optional>("TimestampMode", &_timestamp_mode_str, nullptr,
										[=]() -> std::shared_ptr<ConfigError>
										{
											if (_timestamp_mode_str.UpperCaseString() == "ORIGINAL")
											{
												_timestamp_mode = TimestampMode::Original;
											}
											else if (_timestamp_mode_str.UpperCaseString() == "ZEROBASED")
											{
												_timestamp_mode = TimestampMode::ZeroBased;
											}
											else
											{
												return CreateConfigErrorPtr("Invalid TimestampMode value: %s (expected : UseIncoming, ZeroBased)", _timestamp_mode_str.CStr());
											}
											return nullptr;
										});

						// For backward compatibility
						Register<Optional>("UseIncomingTimestamp", &_use_incoming_timestamp, nullptr,
										[=]() -> std::shared_ptr<ConfigError>
										{
											logw("Config", "UseIncomingTimestamp is deprecated. Please use TimestampMode instead.");
											if (_use_incoming_timestamp)
											{
												_timestamp_mode = TimestampMode::Original;
											}
											else
											{
												_timestamp_mode = TimestampMode::Auto;
											}
											return nullptr;
										});
					}
				};
			}  // namespace pvd
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg