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
#include "./json_object.h"

namespace ov
{
	class Json
	{
	public:
		Json() = delete;
		~Json() = delete;

		static ov::String Stringify(const JsonObject &object);
		static ov::String Stringify(const ::Json::Value &value);
		static ov::String Stringify(const ::Json::Value &value, bool prettify);

		static JsonObject Parse(const ov::String &str);
		static JsonObject Parse(const std::shared_ptr<const Data> &data);
	};
}
