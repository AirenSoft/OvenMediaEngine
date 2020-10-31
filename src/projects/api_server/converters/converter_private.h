//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

#define CONVERTER_RETURN_IF(condition)   \
	if (condition)                       \
	{                                    \
		if (optional == Optional::False) \
		{                                \
			OV_ASSERT2(#condition);      \
		}                                \
                                         \
		return;                          \
	}                                    \
	[[maybe_unused]] Json::Value &object = parent_object[key];

namespace api
{
	namespace conv
	{
		enum class Optional
		{
			True,
			False
		};

		inline void SetString(Json::Value &parent_object, const char *key, const ov::String &value, Optional optional)
		{
			CONVERTER_RETURN_IF(value.IsEmpty());

			object = value.CStr();
		}

		inline void SetInt(Json::Value &parent_object, const char *key, int32_t value)
		{
			parent_object[key] = value;
		}

		inline void SetTimeInterval(Json::Value &parent_object, const char *key, int64_t value)
		{
			parent_object[key] = value;
		}

		inline void SetTimestamp(Json::Value &parent_object, const char *key, const std::chrono::system_clock::time_point &time_point)
		{
			parent_object[key] = "2020-10-31T00:00:00.000Z";
		}

		inline void SetBool(Json::Value &parent_object, const char *key, bool value)
		{
			parent_object[key] = value;
		}
	}  // namespace conv
}  // namespace api